/*
 * _POSIX_C_SOURCE 199309L e necessario para habilitar CLOCK_MONOTONIC,
 * que nao faz parte do padrao C basico mas e definido pelo POSIX.
 * Sem isso o compilador nao reconhece a constante e da erro.
 */
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

/* ============================================================
 * CONSTANTES E DEFINICOES
 * ============================================================ */

#define MAX_SENSORES 1000 /* Maximo de sensores suportados (IDs de 000 a 999) */
#define MAX_LINHA    128  /* Tamanho maximo de uma linha do log               */

/* ============================================================
 * ESTRUTURAS DE DADOS
 * ============================================================ */

/*
 * Struct Sensor - igual a versao sequencial.
 * Guarda os dados acumulados de cada sensor para calcular media e desvio padrao.
 *
 * Formula do desvio padrao incremental (sem guardar todos os valores):
 *   media     = soma / qnt
 *   variancia = (soma_quadrada / qnt) - (media * media)
 *   desvio    = sqrt(variancia)
 */
typedef struct {
    int    id;            /* ID numerico do sensor                                  */
    double soma;          /* Soma acumulada dos valores (para calcular media)        */
    double soma_quadrada; /* Soma dos quadrados (para calcular desvio padrao)        */
    int    qnt;           /* Quantidade de leituras registradas                     */
} Sensor;

/*
 * Struct ArgThread - empacota os argumentos passados para cada thread.
 * Usamos uma struct porque pthread_create so aceita um unico void* de argumento.
 *
 * Estrategia de divisao do trabalho: divisao por bytes.
 *   O arquivo e dividido em fatias de bytes (nao de linhas).
 *   Cada thread recebe [inicio_byte, fim_byte) do arquivo para processar.
 *   Isso e mais eficiente do que contar linhas antes, pois evita uma passagem extra.
 */
typedef struct {
    const char *arquivo;     /* Caminho do arquivo .log (somente leitura, compartilhado) */
    long        inicio_byte; /* Posicao inicial da fatia no arquivo                      */
    long        fim_byte;    /* Posicao final da fatia (-1 = ate o fim do arquivo)       */
} ArgThread;

/* ============================================================
 * VARIAVEIS GLOBAIS COMPARTILHADAS ENTRE AS THREADS
 * ============================================================ */

/* Array de sensores: o indice e o ID do sensor.
 * Ex: "sensor_042" -> sensores[42]
 * Funciona como um mapa de acesso direto por indice. */
Sensor sensores[MAX_SENSORES];

double consumo_total = 0.0; /* Soma de todas as leituras do tipo "energia" */
long   qnt_alertas   = 0;   /* Contador de linhas com status ALERTA ou CRITICO */

/*
 * Mutex global que protege TODOS os dados compartilhados acima.
 *
 * Antes de modificar qualquer variavel global, a thread trava o mutex.
 * Apos a modificacao, destrava. Isso garante que so uma thread por vez
 * acessa os dados, evitando race conditions (condicao de corrida).
 *
 * PTHREAD_MUTEX_INITIALIZER inicializa o mutex estaticamente,
 * sem precisar chamar pthread_mutex_init().
 */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================
 * FUNCAO EXECUTADA POR CADA THREAD
 * ============================================================ */

/*
 * processar_fatia() - processa a fatia do arquivo atribuida a esta thread.
 *
 * Cada thread abre o arquivo de forma independente (FILE* proprio)
 * para evitar conflito de posicao de leitura entre threads.
 */
void *processar_fatia(void *arg) {
    ArgThread *a = (ArgThread *)arg;

    /* PASSO 1: Abrir o arquivo e ir para o byte de inicio da fatia */
    FILE *f = fopen(a->arquivo, "r");
    if (!f) {
        fprintf(stderr, "Erro: thread nao conseguiu abrir '%s'\n", a->arquivo);
        return NULL;
    }
    fseek(f, a->inicio_byte, SEEK_SET);

    /*
     * PASSO 2: Descartar linha parcial (exceto a thread 0).
     *
     * Como dividimos por bytes, o corte pode cair no meio de uma linha:
     *
     *   sensor_001 2026-03-01 00:00:00 tempe|ratura 23.5 status OK\n
     *                                        ^ inicio_byte desta thread
     *
     * A thread descarta "ratura 23.5 status OK\n" com um fgets de lixo,
     * e comeca a processar a partir da proxima linha completa.
     * A linha cortada ja foi processada pela thread anterior.
     */
    if (a->inicio_byte != 0) {
        char lixo[MAX_LINHA];
        fgets(lixo, sizeof(lixo), f);
    }

    /* PASSO 3: Loop principal - processar cada linha da fatia */
    char linha[MAX_LINHA];

    while (1) {
        /* Guarda a posicao ANTES de ler a linha */
        long pos = ftell(f);

        /*
         * Se ja passamos do limite da fatia, encerramos.
         * fim_byte == -1 indica a ultima thread, que vai ate o EOF.
         */
        if (a->fim_byte != -1 && pos >= a->fim_byte) break;

        /* Tenta ler a proxima linha; encerra se chegar no EOF */
        if (!fgets(linha, sizeof(linha), f)) break;

        /* PASSO 4: Parsing da linha com sscanf
         *
         * Formato esperado:
         *   sensor_039 2026-03-01 00:00:00 umidade 55.5 status OK
         *
         *   sensor_%d  -> ID numerico do sensor
         *   %*s %*s    -> data e hora (lidos e DESCARTADOS com *)
         *   %s         -> tipo (temperatura, umidade, energia, corrente, pressao)
         *   %f         -> valor numerico da leitura
         *   status     -> palavra literal ignorada pelo sscanf
         *   %s         -> status (OK, ALERTA, CRITICO)
         *
         * sscanf retorna o numero de campos preenchidos com sucesso (deve ser 4).
         * Se for diferente, a linha esta malformada e e ignorada.
         */
        int   id;
        char  tipo[20];
        float valor;
        char  status[20];

        int campos = sscanf(linha,
                            "sensor_%d %*s %*s %s %f status %s",
                            &id, tipo, &valor, status);

        if (campos != 4)                  continue; /* linha invalida, ignora */
        if (id < 0 || id >= MAX_SENSORES) continue; /* ID fora do intervalo valido */

        /*
         * PASSO 5: Atualizar variaveis globais - SECAO CRITICA
         *
         * pthread_mutex_lock garante exclusao mutua: so uma thread executa
         * este bloco de cada vez. As outras ficam bloqueadas esperando.
         *
         * Agrupamos todos os updates em uma unica secao critica para
         * reduzir o numero de lock/unlock por linha processada.
         */
        pthread_mutex_lock(&mutex);

            /* Acumula dados do sensor (usado para calcular media e desvio padrao) */
            sensores[id].id             = id;
            sensores[id].soma          += valor;
            sensores[id].soma_quadrada += valor * valor;
            sensores[id].qnt           += 1;

            /* Conta ALERTA ('A') e CRITICO ('C') pela primeira letra do status */
            if (status[0] == 'A' || status[0] == 'C') {
                qnt_alertas++;
            }

            /* Soma consumo de energia apenas para leituras do tipo "energia" */
            if (strcmp(tipo, "energia") == 0) {
                consumo_total += valor;
            }

        pthread_mutex_unlock(&mutex);
        /* FIM DA SECAO CRITICA */
    }

    fclose(f);
    return NULL;
}

/* ============================================================
 * FUNCAO PRINCIPAL
 * ============================================================ */

int main(int argc, char *argv[]) {

    /* Validacao dos argumentos.
     * Uso: ./sensor_analyzer_par <num_threads> <arquivo.log> */
    if (argc < 3) {
        printf("Uso: %s <num_threads> <arquivo.log>\n", argv[0]);
        return 1;
    }

    int         num_threads = atoi(argv[1]);
    const char *arquivo     = argv[2];

    if (num_threads <= 0) {
        printf("Erro: numero de threads deve ser maior que 0\n");
        return 1;
    }

    /* PASSO 1: Inicializar array de sensores com zeros.
     * memset zera todos os bytes, evitando lixo de memoria em soma,
     * soma_quadrada e qnt (que precisam comecar em 0). */
    memset(sensores, 0, sizeof(sensores));

    /* PASSO 2: Iniciar medicao de tempo */
    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    /* PASSO 3: Descobrir o tamanho total do arquivo em bytes.
     * fseek(SEEK_END) vai para o final; ftell retorna o offset = tamanho. */
    FILE *f = fopen(arquivo, "r");
    if (!f) {
        printf("Erro: nao foi possivel abrir '%s'\n", arquivo);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long tamanho_arquivo = ftell(f);
    fclose(f);

    /* PASSO 4: Calcular a fatia de cada thread.
     *
     * Divisao por bytes: fatia = tamanho / num_threads
     * A ultima thread recebe o restante (fim_byte = -1 => ate o EOF),
     * absorvendo os bytes excedentes da divisao inteira.
     *
     * Exemplo com 4 threads em um arquivo de 1000 bytes:
     *   Thread 0: bytes [  0, 250)
     *   Thread 1: bytes [250, 500)
     *   Thread 2: bytes [500, 750)
     *   Thread 3: bytes [750, EOF)
     */
    long fatia = tamanho_arquivo / num_threads;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    ArgThread *args    = malloc(num_threads * sizeof(ArgThread));

    if (!threads || !args) {
        printf("Erro: falha ao alocar memoria\n");
        return 1;
    }

    /* PASSO 5: Criar as threads.
     *
     * pthread_create dispara cada thread imediatamente.
     * Todas comecam a executar processar_fatia() de forma concorrente.
     *
     * Assinatura: pthread_create(id, atributos, funcao, argumento)
     *   - NULL nos atributos usa as configuracoes padrao */
    for (int i = 0; i < num_threads; i++) {
        args[i].arquivo     = arquivo;
        args[i].inicio_byte = (long)i * fatia;
        args[i].fim_byte    = (i == num_threads - 1) ? -1L : (long)(i + 1) * fatia;

        pthread_create(&threads[i], NULL, processar_fatia, &args[i]);
    }

    /* PASSO 6: Aguardar todas as threads terminarem.
     *
     * pthread_join bloqueia o main ate que a thread i encerre.
     * Sem o join, o main poderia terminar antes das threads,
     * encerrando o processo inteiro no meio do processamento. */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* PASSO 7: Finalizar medicao de tempo */
    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo = (fim.tv_sec  - inicio.tv_sec)
                 + (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    /* ============================================================
     * EXIBICAO DOS RESULTADOS
     * ============================================================ */

    /* Medias por sensor - exibe apenas os 10 primeiros com leituras */
    printf("\n=== Medias por sensor (primeiros 10) ===\n");
    int exibidos = 0;
    for (int i = 0; i < MAX_SENSORES && exibidos < 10; i++) {
        if (sensores[i].qnt > 0) {
            double media = sensores[i].soma / sensores[i].qnt;
            printf("  sensor_%03d: %.2f\n", i, media);
            exibidos++;
        }
    }

    /* Sensor mais instavel: maior desvio padrao entre todos os sensores.
     *
     * Formula incremental (nao precisa reler os dados):
     *   variancia = (soma_quadrada / n) - (soma/n)^2
     *   desvio    = sqrt(variancia)
     */
    int    sensor_instavel = -1;
    double maior_desvio    = -1.0;

    for (int i = 0; i < MAX_SENSORES; i++) {
        if (sensores[i].qnt > 1) {
            double n         = (double)sensores[i].qnt;
            double media     = sensores[i].soma / n;
            double variancia = (sensores[i].soma_quadrada / n) - (media * media);
            if (variancia < 0.0) variancia = 0.0; /* corrige erro de ponto flutuante */
            double desvio    = sqrt(variancia);

            if (desvio > maior_desvio) {
                maior_desvio    = desvio;
                sensor_instavel = i;
            }
        }
    }

    printf("\n=== Sensor mais instavel ===\n");
    if (sensor_instavel >= 0)
        printf("  sensor_%03d  |  desvio padrao: %.4f\n", sensor_instavel, maior_desvio);
    else
        printf("  Nenhum sensor com leituras suficientes.\n");

    printf("\n=== Totais ===\n");
    printf("  Alertas (ALERTA + CRITICO): %ld\n", qnt_alertas);
    printf("  Consumo total de energia:   %.2f W\n", consumo_total);

    printf("\n=== Desempenho ===\n");
    printf("  Threads utilizadas: %d\n", num_threads);
    printf("  Tempo de execucao:  %.4f segundos\n", tempo);

    /* Liberar memoria alocada */
    free(threads);
    free(args);

    return 0;
}

/*
 * REFERENCIAS:
 * https://man7.org/linux/man-pages/man3/pthread_create.3.html
 * https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html
 * https://man7.org/linux/man-pages/man3/fseek.3.html
 * https://stackoverflow.com/questions/48332332/what-does-define-posix-source-mean
 */
