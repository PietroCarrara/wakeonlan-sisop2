# Ambiente de Desenvolvimento

O auto-complete e o formatter já estão configurados para IDE VSCode.

# Rodando o programa

### Compilação

- Basta rodar o seguinte comando para gerar o executável, ele irá compilar todos os arquivos e o resultado irá pasta `build`

```sh
 make
```

- Caso queira limpar a pasta com os outputs, execute 

```sh
make clear_build
```

### Execução
- **Recomendado**: Para criar uma instância de um cliente, execute

```sh
make client
```

- **Recomendado**: Para criar uma instância de um gerente, execute

```sh
make manager
```

- É possível também apenas chamar o executável do programa para criar um cliente

```sh
./build/wakeonlan 
```

- Para criar o gerente, execute o mesmo comando com parâmetro correto

```sh
./build/wakeonlan manager 
```


***

# Padrões de Código

- Sempre que possível, isole os componentes em classes separadas, agrupe funcionalidades que poderiam ser fornecidas para um mesmo recurso.
- Faça um arquivo para cada classe, com os nomes `class_name.h` e `class_name.cpp`. É necessário apenas importar a nova bilioteca local no arquivo principal, a compilação feita pelo make se encarrega de compilar e linkar todos os arquivos. 
- Especificamente sobre os headers `class_name.h`, devem possuir as definições de tipos e protótipos de funções, lembrando que **funções com template devem ficar no header**. 
- Não use ponteiros, use referências. O compilador é mais exigente com elas e você ganha a garantia que elas nunca serão nulas, pegando coisas que poderiam dar um `NullPointerException` em tempo de compilação.
  - Então no lugar de declarar `int* ponteiro`, use `int& referencia`.
- Não jogue `exceptions`. Em código multithreaded isso vira mais infernal que em código normal. Se sua função tem chance de falhar e não pode retornar o resultado esperado, mude seu retorno para usar um `optional`, por exemplo:

```cpp
  Status get_status_from_host() { ... }
```

Deve ser transformado em:

```cpp
optional<Status> get_status_from_host() { ... }
```
***
> *"Eu preciso de uma thread que gera dados e uma que consome esses dados, como eu faço?"* 

Pra resolver o problema do produtor/consumidor, usamos `canais` sempre como primeira opção (padrão visto em [golang](https://golangdocs.com/channels-in-golang)). Canais são uma maneira de manter comunicação entre processos paralelos sem danificar a sanidade mental de quem está programando. Uma task põe mensagens no canal e outra vai retirando. Usa-se assim:
1. A função *"pai"* dos processos cria um canal e o passa como referência para duas (ou mais) funções paralelas
2. Quando uma função põe dados no canal, caso ele já esteja carregando algum dado, ela fica bloqueada até que alguém pegue aquele dado na outra ponta.
3. Quando uma função tenta pegar dados no canal, se não há nenhum, ela fica bloqueada até que alguém ponha algum dado na outra ponta.