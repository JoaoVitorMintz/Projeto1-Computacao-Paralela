/*
Pelo que pesquisei, serve para permitir o uso de mais funcionalidades que não fazem parte da biblioteca
padrão do C, tive que colocar para o CLOCK_MONOTONIC ser definido.
*/

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>

#define MAX_SENSORES 1000

/*
ESTRATÉGIA DE OTIMIZAÇÃO - ACUMULADORES LOCAIS:

Na versão com mutex (sensor_analyzer_par.c), o maior problema é que todas as threads
disputam o mesmo mutex a cada linha lida, causando um problema onde enquanto uma thread
está com o lock, as outras ficam esperando oque faz perder o benefício do paralelismo que estamos procurando.

A solução que nós encontramos foi: cada thread trabalha com suas PRÓPRIAS cópias locais das estruturas
de dados (um array de sensores local, um consumo_total local e um qnt_alertas local).
Durante todo o processamento, nenhuma thread precisa de mutex, pois não há dados
compartilhados sendo modificados. Só ao final, quando todas terminaram, o programa
principal faz a fusão (merge) dos resultados locais no resultado global.

Resultado: zero contenção de mutex durante o processamento, apenas uma fusão simples
e rápida no final.
*/

// Struct para calculo de tempo e informações de sensores que deverão ser exibidas e para separar threads
struct timespec inicio, fim;
typedef struct {
    int id;
    double soma;
    double soma_quadrada; // Para cálculo do Desvio Padrão
    int qnt;
} Sensores;

typedef struct {
    int thread_id;
    char* file_name;
    long inicio;
    long fim;
    // Acumuladores locais: cada thread tem sua própria cópia, sem compartilhamento
    Sensores* local_s;
    double local_consumo;
    double local_alertas;
} ThreadArgs;

// Variáveis globais para resultado final (só escritas após o join, sem mutex necessário)
double consumo_total = 0;
double qnt_alertas = 0;
Sensores* s;

// Função para pegar valores e serem inseridos nos acumuladores LOCAIS da thread
void* extrair_valores(void* arg) {
    ThreadArgs* args = (ThreadArgs*) arg;
    char linha[90]; // Vetor para armazenar a linha que será analisada

    // Inicializa acumuladores locais da thread com zero
    args->local_consumo = 0;
    args->local_alertas = 0;
    // local_s já foi alocado com calloc no main, então já está zerado

    // Abre o arquivo para cada thread ler o seu bloco de informação
    FILE* file = fopen(args->file_name, "r");
    if (file == NULL) {
        printf("ERRO: Arquivo log não pode ser aberto dentro da thread");
        return NULL;
    }

    int linha_atual = 0;

    // Encontrando a posição inicial para começar a leitura do bloco da thread
    while (linha_atual < args->inicio && fgets(linha, sizeof(linha), file)) {
        linha_atual++;
    }

    while (linha_atual < args->fim && fgets(linha, sizeof(linha), file)) {

        // Informações necessárias na coleta:
        int id;
        char status[20]; // OK, ALERTA e CRITICO
        char tipo[20];   // TEMPERATURA, UMIDADE, ENERGIA, CORRENTE E PRESSÃO
        float valor;     // Valor medido pelo sensor

        // EXEMPLO DE LINHA: sensor_039 2026-03-01 00:00:00 umidade 55.5 status OK
        sscanf(linha, "sensor_%d %*s %*s %s %f status %s", &id, tipo, &valor, status);

        if (id < MAX_SENSORES) {
            // SEM MUTEX: cada thread escreve apenas no seu próprio local_s
            Sensores* sensor = &args->local_s[id]; // Gera sensor com as informações salvas na struct

            sensor->id = id;
            sensor->soma += valor;
            sensor->soma_quadrada += valor * valor;
            sensor->qnt += 1;

            // Pega a primeira letra da string e compara, A para ALERTA e C para CRITICO
            if (status[0] == 'A' || status[0] == 'C') {
                args->local_alertas += 1;
            }

            // Usando string compare para ver se o tipo é energia para somar ao consumo local
            if (strcmp(tipo, "energia") == 0) {
                args->local_consumo += valor;
            }
        }
        linha_atual++;
    }

    fclose(file);
    return NULL;
}


// Função que vai calcular o desvio padrão de um sensor usando soma e soma dos quadrados
double calcula_desvio_padrao(Sensores* s) {
    double media = s->soma / s->qnt;
    return sqrt((s->soma_quadrada / s->qnt) - (media * media));
}


int main(int argc, char **argv) {
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    // Verifica quantos argv existem (deve haver apenas 2, o numero de threads e o arquivo)
    if (argc < 3) {
        printf("Uso: %s <num_threads> <arquivo>\n", argv[0]);
        return 1;
    }

    // Configuração para, caso use o ./sensor_analyzer_par_otimizado <num_threads> <arquivo>, ele leia corretamente
    int n_threads = atoi(argv[1]); // atoi recebe string e transforma em int, vem do stdlib.h
    FILE* file = fopen(argv[2], "r");

    // Verifica se foi aberto corretamente
    if (file == NULL) {
        printf("ERRO: Arquivo log não pode ser aberto");
        return 1;
    }

    int total_linhas = 0;
    char linha[90];

    // Conta a quantidade de linhas para separar por thread
    while (fgets(linha, sizeof(linha), file)) {
        total_linhas++;
    }

    int linhas_por_thread = total_linhas / n_threads;

    // Uso de calloc para inicializar array sem lixo de memória (usando malloc calculava errado o desvio padrão)
    s = (Sensores*)calloc(MAX_SENSORES, sizeof(Sensores));

    pthread_t threads[n_threads];
    ThreadArgs args[n_threads];

    // Criando threads e enviando para processar o extrair_valores
    // Cada thread também recebe seu próprio array local alocado com calloc (sem necessidade de mutex)
    for (int i = 0; i < n_threads; i++) {
        args[i].thread_id = i;
        args[i].inicio = i * linhas_por_thread;

        if (i == n_threads - 1) {
            args[i].fim = total_linhas;
        } else {
            args[i].fim = (i + 1) * linhas_por_thread;
        }

        args[i].file_name = argv[2];

        // Cada thread recebe sua própria cópia zerada do array de sensores
        args[i].local_s = (Sensores*)calloc(MAX_SENSORES, sizeof(Sensores));

        pthread_create(&threads[i], NULL, extrair_valores, &args[i]);
    }

    // Dando join nos valores calculados em extrair_valores
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // MERGE: fusão dos resultados locais de cada thread no resultado global
    // Feito sequencialmente no main, sem necessidade de mutex (threads já terminaram)
    for (int i = 0; i < n_threads; i++) {
        consumo_total += args[i].local_consumo;
        qnt_alertas   += args[i].local_alertas;

        for (int j = 0; j < MAX_SENSORES; j++) {
            if (args[i].local_s[j].qnt > 0) {
                s[j].id = j;
                s[j].soma          += args[i].local_s[j].soma;
                s[j].soma_quadrada += args[i].local_s[j].soma_quadrada;
                s[j].qnt           += args[i].local_s[j].qnt;
            }
        }

        // Libera o array local da thread após o merge
        free(args[i].local_s);
    }

    fclose(file);

    // Encontrar o sensor mais instável procurando qual tem o maior desvio padrão
    Sensores* sensor_instavel = NULL;
    double maior_desvio = 0.0;

    for (int i = 0; i < MAX_SENSORES; i++) {
        if (s[i].qnt > 0) {
            double desvio = calcula_desvio_padrao(&s[i]);

            if (desvio > maior_desvio) {
                maior_desvio = desvio;
                sensor_instavel = &s[i];
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);

    printf("\n--- Média de Temperatura por Sensor (Primeiros 10) ---\n");   // Exibe a Média de Temperatura por Sensor (Primeiros 10 como pedido)
    for (int i = 0; i <= 10; i++) {
        if (s[i].qnt > 0) {
            printf("Sensor_%03d: Média = %.2f\n", s[i].id, s[i].soma / s[i].qnt);
        }
    }

    printf("\nSensor mais instável: sensor_%03d (Desvio Padrão: %.2f)\n", sensor_instavel->id, maior_desvio); // Exibe o sensor mais instável ( com o nome e desvio padrão)

    printf("Total de alertas: %.0f\n", qnt_alertas); // Exibe o total de alertas no log

    printf("Consumo total de energia: %.2f\n", consumo_total); // Exibe consumo total de energia gasta

    double tempo = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9;
    printf("Tempo: %f segundos\n", tempo); //Exibe o tempo que foi necessário para concluir

    free(s); // Liberar ponteiro de structs

    return 0;
}
/*
REFERÊNCIAS:
https://stackoverflow.com/questions/48332332/what-does-define-posix-source-mean
https://stackoverflow.com/questions/40515557/compilation-error-on-clock-gettime-and-clock-monotonic]
https://www.freecodecamp.org/portuguese/news/manipulacao-de-arquivos-em-c-como-abrir-e-fechar-arquivos-e-escrever-algo-neles/
https://www.ibm.com/docs/pt-br/i/7.5.0?topic=functions-fgets-read-string
https://www.youtube.com/watch?v=BygicGzFCRw
*/
