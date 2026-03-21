/*
Pelo que pesquisei, serve para permitir o uso de mais funcionalidades que não fazem parte da biblioteca
padrão do C, tive que colocar para o CLOCL_MONOTONIC ser definido.
*/
#define _POSIX_C_SOURCE 199309L 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

    // 
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

        consumo_total += valor;
    }
}

double calcula_desvio_padrao() {
    return 0.0;
}

int main() {
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    FILE *file = fopen("sensores.log", "r"); // Abrir arquivo .log

    if (file == NULL) {
        printf("ERRO: Arquivo log não pode ser aberto");
        return 1;
    }

    Sensores* s = (Sensores*)malloc(MAX_SENSORES * sizeof(Sensores*));

    extrair_valores(file, s);

    fclose(file);

    clock_gettime(CLOCK_MONOTONIC, &fim);

    double tempo = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9;
    printf("Tempo: %f segundos", tempo);

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