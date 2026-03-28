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

#define MAX_SENSORES 1000

// Variáveis globais
double consumo_total = 0;
double qnt_alertas = 0;

// Struct para calculo de tempo e informações de sensores que deverão ser exibidas
struct timespec inicio, fim;
typedef struct {
    int id;
    double soma;
    double soma_quadrada; // Para cálculo do Desvio Padrão
    int qnt;
} Sensores;

// Função para pegar valores e serem inseridos na struct sensores e nas variáveis globais
void extrair_valores(FILE *file, void* sensores) {
    char linha[90]; // Vetor para armazenar a linha que será analisada
    Sensores* array = (Sensores*)sensores; // Array de sensores com acesso por id

    // While que passará por TODAS as linhas do arquivo e coletará os dados necessários
    while (fgets(linha, sizeof(linha), file)) {
        // Informações necessárias na coleta:
        int id;
        char status[20]; // OK, ALERTA e CRITICO
        char tipo[20]; // TEMPERATURA, UMIDADE, ENERGIA, CORRENTE E PRESSÃO
        float valor; // Valor medido pelo sensor

        // EXEMPLO DE LINHA: sensor_039 2026-03-01 00:00:00 umidade 55.5 status OK
        sscanf(linha, "sensor_%d %*s %*s %s %f status %s", &id, tipo, &valor, status);

        Sensores* s = &array[id]; // Gera sensor com as informações salvas na struct

        s->id = id;
        s->soma += valor;
        s->soma_quadrada += valor * valor;
        s->qnt += 1;

        // Pega a primeira letra da string e compara, A para ALERTA e C para CRITICO
        if (status[0] == 'A' || status[0] == 'C') {
            (qnt_alertas) += 1;
        }

        // Usando string compare para ver se o tipo é energia para somar ao consumo total
        if (strcmp(tipo, "energia") == 0) {
            consumo_total += valor;
        }

    }
}
 // Função que vai calcular o desvio padrão de um sensor usando soma e soma dos quadrados
double calcula_desvio_padrao(Sensores* s) {
    double media = s->soma / s->qnt;
    return sqrt((s->soma_quadrada / s->qnt) - (media * media));
}

int main() {
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    FILE *file = fopen("sensores.log", "r"); // Abrir arquivo .log

    if (file == NULL) {
        printf("ERRO: Arquivo log não pode ser aberto");
        return 1;
    }

    // Uso de calloc para inicializar array sem lixo de memória (usando malloc calculava errado o desvio padrão)
    Sensores* s = (Sensores*)calloc(MAX_SENSORES, sizeof(Sensores));

    extrair_valores(file, s);

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

    fclose(file);

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

*/
