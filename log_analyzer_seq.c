#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct timespec start, end;

// Função para pegar os valores de Status Code e Bytes Sent (Só uma ideia, precisa fazer funcionar)
void extract_info(void *fp, char* line) {
    char *quote_ptr = strstr(line, "\" "); // Encontra o " depois da requisição

    if (quote_ptr) {
        int status_code;
        long long bytes_sent;
        if (sscanf(quote_ptr + 2, "%d %lld", &status_code, &bytes_sent) == 2) {
            // Agora recebe valores em status_code e bytes_sent
        }
    }
}

int main() {
    FILE *fp = fopen("acess_log_large.txt", "r");

    clock_gettime(CLOCK_MONOTONIC, &start);

    if (fp == NULL) {
        perror("ERRO: Não foi possível abrir o arquivo.");
        exit(EXIT_FAILURE);
    }

    char *line = NULL;
    size_t len = 0;
    __ssize_t read;

    while ((read = getline(&line, &len, fp)) != -1) {
        // Processas a 'line' aqui...
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calcula o tempo que demorou para processar
    double time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Tempo de execução: %.4f segundos\n", time_spent); 

    fclose(fp);
    if (line) {
        free(line);
    }

    return 0;
}