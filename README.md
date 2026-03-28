# Projeto 1 - Computação Paralela

## 👥 Alunos:
 - João Vitor Garcia Aguiar Mintz
 - Mateus Kage Moya

## ✅ Objetivos do projeto:

Desenvolver uma aplicação em C que analisa logs de sensores conectados em uma rede IoT. O ponto principal é que: haverá dois tipos de scripts, um que paraleliza em threads esta leitura, controlado via Mutex e outro que lê de forma sequencial e, ao final, será comparado o tempo entre elas.

## ⚙️ Como rodar:

Dentro do termial no ambiente Linux/UNIX:

```Bash
make
./sensor_analyzer_seq
./sensor_analyzer_par <num_threads> <arquivo_leitura>
/sensor_analyzer_par_otimizado <num_threads> <arquivo_leitura>
```

## 📝 Explicação geral da lógica do código:

### Análise dos sensores de forma sequencial:

Dentro do código, será gerado um array com **MAX_SENSORES** alocado dinamicamente para armazenar structs que guardarão os dados de cada sensor por ID. Seguindo a seguinte lógica: o arquivo entra em **extrair_valores(file, s)** e, dentro dessa função, cada linha é analisada individualmente. Por exemplo, se a linha 1 contiver sensor_002, isso significa que o ID do sensor é 2 e, no array, no índice 2, estão os dados desse sensor que, caso apareça novamente em outra linha, terão seus valores acumulados em qnt, soma e soma_quadrada, sendo essas informações necessárias para desenvolver a lógica solicitada pelo professor. Resumindo: cada posição do array representa um sensor específico, identificado pelo seu ID, funcionando de forma semelhante a um **map**, porém com implementação baseada em um vetor com acesso direto aos índices.

### Análise dos sensores de forma paralela não otimizada:

A versão paralela mantém a mesma lógica de armazenamento da versão sequencial: um vetor onde o índice representa o ID do sensor, e cada posição armazena a struct com suas informações agregadas. A principal diferença está na forma como o processamento é distribuído entre múltiplas threads. Inicialmente, são passados via terminal o número de threads e o nome do arquivo de entrada. Em seguida, o programa conta o número total de linhas do arquivo e divide esse total entre as threads, definindo um intervalo de linhas para cada uma. Cada thread abre o arquivo de forma independente e avança até a linha inicial correspondente ao seu bloco, processando apenas o intervalo designado. Durante o processamento, como as threads compartilham estruturas globais (vetor de sensores, contador de alertas e consumo total), é necessário o uso de mutex para garantir exclusão mútua durante as atualizações, evitando condições de corrida (race conditions). Apesar da leitura do arquivo ser paralelizada, o uso de mutex na atualização dos dados compartilhados ainda pode gerar contenção, pois múltiplas threads disputam o mesmo lock, reduzindo o paralelismo efetivo e impactando o desempenho.

## 🔢 Lógica dos arrays:
 - Sensores s* → Vetor global onde cada índice representa o ID do sensor, armazenando os dados agregados (soma, soma dos quadrados e quantidade de leituras).
 - ThreadArgs args → Vetor de structs onde cada posição contém os parâmetros necessários para execução de uma thread, como seu identificador, o nome do arquivo e os limites (linha inicial e final) do bloco que ela deve processar.
 - pthread_t threads → Vetor que armazena os identificadores das threads, permitindo posteriormente sincronizar sua execução com pthread_join.

## 📖 Para compreender melhor o projeto:
 - Leia o arquivo "AnalisarLogs_IoT.pdf" para mais detalhes sobre o projeto e sua estrutura.