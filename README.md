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
./<nome-executável>
```

## 📝 Explicação geral da lógica do código:

### Análise dos sensores de forma sequencial:

Dentro do código, será gerado um array com **MAX_SENSORES** alocado dinamicamente para armazenar structs que guardarão os dados de cada sensor por ID. Seguindo a seguinte lógica: o arquivo entra em **extrair_valores(file, s)** e, dentro dessa função, cada linha é analisada individualmente. Por exemplo, se a linha 1 contiver sensor_002, isso significa que o ID do sensor é 2 e, no array, no índice 2, estão os dados desse sensor que, caso apareça novamente em outra linha, terão seus valores acumulados em qnt, soma e soma_quadrada, sendo essas informações necessárias para desenvolver a lógica solicitada pelo professor. Resumindo: cada posição do array representa um sensor específico, identificado pelo seu ID, funcionando de forma semelhante a um **map**, porém com implementação baseada em um vetor com acesso direto aos índices.

## 📖 Para compreender melhor o projeto:
 - Leia o arquivo "AnalisarLogs_IoT.pdf" para mais detalhes sobre o projeto e sua estrutura.