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
O QUE PRECISA DE MUTEX (POR SER COMPARTILHADO):
- Sensores* s;
- double consumo_total;
- double qnt_alertas;
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
} ThreadArgs;

// Variáveis globais
double consumo_total = 0;
double qnt_alertas = 0;
Sensores* s; // Agora Sensores* s é global para todas as threads terem acesso
pthread_mutex_t mutex;

// Função para pegar valores e serem inseridos na struct sensores e nas variáveis globais
void* extrair_valores(void* arg) {
    ThreadArgs* args = (ThreadArgs*) arg;
    char linha[90]; // Vetor para armazenar a linha que será analisada

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
        char tipo[20]; // TEMPERATURA, UMIDADE, ENERGIA, CORRENTE E PRESSÃO
        float valor; // Valor medido pelo sensor

        // EXEMPLO DE LINHA: sensor_039 2026-03-01 00:00:00 umidade 55.5 status OK
        sscanf(linha, "sensor_%d %*s %*s %s %f status %s", &id, tipo, &valor, status);

        if (id < MAX_SENSORES) {
            // Dando lock no mutex para evitar o acesso de outra thread
            pthread_mutex_lock(&mutex);

            Sensores* sensor = &s[id]; // Gera sensor com as informações salvas na struct

            sensor->id = id;
            sensor->soma += valor;
            sensor->soma_quadrada += valor * valor;
            sensor->qnt += 1;

            // Pega a primeira letra da string e compara, A para ALERTA e C para CRITICO
            if (status[0] == 'A' || status[0] == 'C') {
                (qnt_alertas) += 1;
            }

            // Usando string compare para ver se o tipo é energia para somar ao consumo total
            if (strcmp(tipo, "energia") == 0) {
                consumo_total += valor;
            }

            // Liberando mutex
            pthread_mutex_unlock(&mutex);
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

    // Configuração para, caso use o ./sensor_analyzer_par <num_threads> <arquivo>, ele leia corretamente
    // parâmetro na linha de comando
    int n_threads = atoi(argv[1]); // atoi recebe string e transforma e int, vem do stdlib.h
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

    pthread_mutex_init(&mutex, NULL);

    // Criando threads e enviando para processar o extrair_valores
    for (int i = 0; i < n_threads; i++) {
        args[i].thread_id = i;
        args[i].inicio = i * linhas_por_thread;

        if (i == n_threads - 1) {
            args[i].fim = total_linhas;
        } else {
            args[i].fim = (i + 1) * linhas_por_thread;
        }

        args[i].file_name = argv[2];

        pthread_create(&threads[i], NULL, extrair_valores, &args[i]);
    }
    
    // Dando join nos valores cálculados em extrair_valores
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);

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
    
    printf("Consumo total de energia: %.2fWh\n", consumo_total); // Exibe consumo total de energia gasta

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
