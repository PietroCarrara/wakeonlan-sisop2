## Ambiente de Desenvolvimento

O auto-complete e o comando de compilação já estão configurados pro VSCode, é só abrir a pasta.

### Rodando

Apertando F5, o VSCode compila e roda o programa em modo de debug. Para mudar se você quer rodar o cliente ou o servidor, navegue até o menu de debugging e selecione a configuração desejada:

![](./docs/debugging.png)

## Padrões de Código

O projeto não vai ser um troço enorme, então isso são menos regras e mais sugestões para tornar nossa vida menos miserável usando C++. Se sentir que burlá-los uma vez ou outra vai nos dar menos trabalho, vá em frente!

- Não use ponteiros, use referências no lugar. Elas são mais "reclamonas", e você ganha a garantia que elas serão nulas, pegando coisas que poderiam dar um `NullPointerException` em tempo de compilação.
  - Então no lugar de declarar `int* ponteiro`, use `int& referencia`.
- Não jogue exceptions. Em código multithreaded isso vira mais infernal que em código normal. Se sua função tem chance de falhar e não pode retornar o resultado esperado, mude seu retorno para usar um `optional`.
  - `Status get_status_from_host() { ... }` vira `optional<Status> get_status_from_host() { ... }`.
- *Eu preciso de uma thread que gera dados e uma que consome esses dados, como eu faço?* Pra resolver o problema do produtor/consumidor, vamos tentar usar **canais** até não dar mais (eu não sei onde surgiu a ideia, mas eu vi pela primeira vez em [golang](https://golangdocs.com/channels-in-golang)). São uma maneira de manter comunicação entre processos paralelos sem danificar a sanidade mental de quem está programando. Uma task põe coisas dentro dele, e outra vai tirando. Usa-se assim:
  - A função "pai" dos processos cria um canal, e o passa como referência para duas (ou mais) funções paralelas
  - Quando uma função põe dados no canal, ela fica bloqueada até que alguém pegue aquele dado na outra ponta.
  - Quando uma função tenta pegar dados no canal, se não há nenhum, ela fica bloqueada até que alguém ponha algum dado na outra ponta.

## Separação em Arquivos

Compilar projetos em C++ é uma merda. Pra evitar os traumas de tentar montar um sistema de build complexo, vamos seguir uma regra: você pode criar sim mais arquivos, mas eles têm de ser no formato `.hpp`, e **NÃO PODEM INCLUIR OUTROS ARQUIVOS LOCAIS**. Ou seja, se você definiu o tipo `HostsTable` em `HostsTable.hpp`, apenas o `main.cpp` pode dar `#include "HostsTable.hpp"`. Isso é uma merda e super limitante? Sim, mas evita a dor de configurar um Makefile.