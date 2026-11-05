# Tabela Hash (string → int) em C

[![CI](https://github.com/sidsidoso/tabela-hash-c/actions/workflows/ci.yml/badge.svg)](https://github.com/sidsidoso/tabela-hash-c/actions/workflows/ci.yml)
![Linguagem](https://img.shields.io/badge/linguagem-C11-blue.svg)
[![Licença: MIT](https://img.shields.io/badge/licen%C3%A7a-MIT-green.svg)](LICENSE)

Implementação de uma **tabela hash** que associa **strings** (chaves) a
**inteiros** (valores), em C11, usando **endereçamento aberto com sondagem
linear**, função de hash **FNV-1a**, **redimensionamento automático** pelo
fator de carga (0,75) e remoção com **lápides** (*tombstones*).
Projeto desenvolvido para a disciplina de **Estrutura de Dados**.

---

## Sumário

- [Conceito](#conceito)
- [Por que endereçamento aberto?](#por-que-endereçamento-aberto)
- [Operações e complexidade](#operações-e-complexidade)
- [Decisões de implementação e otimizações](#decisões-de-implementação-e-otimizações)
- [Estrutura de pastas](#estrutura-de-pastas)
- [Como compilar, executar e testar](#como-compilar-executar-e-testar)
- [Exemplo de saída](#exemplo-de-saída)
- [Referências](#referências)

---

## Conceito

Uma **tabela hash** guarda pares chave → valor em um vetor. Uma **função de
hash** transforma a chave em um número, que é reduzido a um índice do vetor:

```
 índice = hash(chave) mod capacidade        (aqui: hash & (capacidade - 1))
```

Se a função espalha bem as chaves, buscar, inserir e remover custam O(1) em
média — sem comparar a chave com todos os elementos.

### Função de hash FNV-1a

Para cada byte da string: faz XOR do byte no hash e multiplica pelo primo FNV.

```c
uint32_t h = 2166136261u;          /* offset basis */
for (cada byte b da chave) {
    h ^= b;
    h *= 16777619u;                /* primo FNV de 32 bits */
}
```

É simples, rápida e tem boa dispersão para strings curtas.

### Colisões e sondagem linear

Duas chaves diferentes podem cair no mesmo índice (**colisão**). Na
**sondagem linear**, se a posição estiver ocupada, tenta-se a seguinte
(`i + 1`, `i + 2`, ...), dando a volta no fim do vetor:

```
 capacidade = 8          inserir "rei" (hash -> 3), mas 3 e 4 ocupados

  0       1       2       3       4       5       6       7
 +-------+-------+-------+-------+-------+-------+-------+-------+
 |       | "do"  |       | "o"   | "a"   | "rei" |       |       |
 +-------+-------+-------+-------+-------+-------+-------+-------+
                           ^ 1ª    ^ 2ª    ^ 3ª tentativa: livre
```

### Remoção com lápides (*tombstones*)

Não se pode simplesmente esvaziar o slot removido: isso "quebraria" a
sequência de sondagem de chaves que passaram por ele. Por isso o slot vira
uma **lápide** (`X`): a busca **continua** através dela, e a inserção pode
**reaproveitá-la**.

```
 remover "o" (índice 3):

 | "do"  |       |   X   | "a"   | "rei" |
     1       2       3       4       5
 buscar "rei": 3 (lápide, segue) -> 4 (segue) -> 5 (achou)
```

### Redimensionamento

O **fator de carga** é α = (ocupados + lápides) / capacidade. Antes de inserir
uma chave nova, se α passaria de **0,75**, a tabela é reconstruída:

- se a maior parte dos slots usados é de chaves vivas → capacidade **dobra**;
- se a maior parte é de lápides → reconstrói com a **mesma** capacidade
  (apenas limpa as lápides).

## Por que endereçamento aberto?

A alternativa seria **encadeamento separado** (cada posição do vetor aponta
para uma lista ligada). Escolhemos **endereçamento aberto com sondagem linear** porque:

1. **Localidade de cache:** todos os slots ficam em um único vetor contíguo;
   a sondagem linear percorre posições vizinhas, que já estão na mesma linha
   de cache. No encadeamento cada nó é um `malloc` separado, espalhado na memória.
2. **Menos alocações:** não há um nó por elemento — só o vetor de slots
   (e a cópia de cada chave, que existiria nos dois esquemas).
3. **Menos memória por elemento:** não há ponteiro `proximo` em cada entrada.
4. **Desempenho previsível com α ≤ 0,75:** pela análise de Knuth, o número
   esperado de sondagens em uma busca bem-sucedida é ≈ ½(1 + 1/(1 − α)),
   cerca de 2,5 com α = 0,75.

As desvantagens — sensibilidade a agrupamento (*clustering*) e a necessidade
de lápides na remoção — são controladas pelo limite de 0,75, pela boa
dispersão do FNV-1a e pela limpeza de lápides no redimensionamento.

## Operações e complexidade

*n* = número de pares, *c* = capacidade, *k* = tamanho da chave.

| Operação               | Função          | Tempo médio            | Pior caso | Espaço extra |
|------------------------|-----------------|------------------------|-----------|--------------|
| Criar                  | `th_criar`      | O(c)                   | O(c)      | O(c)         |
| Destruir               | `th_destruir`   | O(c)                   | O(c)      | O(1)         |
| Inserir / atualizar    | `th_inserir`    | O(k) amortizado*       | O(n + k)  | O(k)         |
| Buscar                 | `th_buscar`     | O(k)                   | O(n · k)  | O(1)         |
| Contém                 | `th_contem`     | O(k)                   | O(n · k)  | O(1)         |
| Remover                | `th_remover`    | O(k)                   | O(n · k)  | O(1)         |
| Tamanho / capacidade   | `th_tamanho`, `th_capacidade` | O(1)     | O(1)      | O(1)         |
| Percorrer              | `th_percorrer`  | O(c)                   | O(c)      | O(1)         |
| Hash FNV-1a            | `th_fnv1a`      | O(k)                   | O(k)      | O(1)         |

\* O redimensionamento custa O(c), mas como a capacidade dobra, o custo
amortizado por inserção continua O(1) (desconsiderando o tamanho da chave).
Em termos do número de elementos, todas as operações são **O(1) esperado**.

Espaço total: **O(n)** slots (c ≤ 8n/3 após crescer) + as cópias das chaves.

## Decisões de implementação e otimizações

- **Capacidade potência de 2:** o índice é `hash & (capacidade - 1)`, sem `%`.
- **Hash armazenado em cada slot:** no redimensionamento não é preciso
  recalcular o hash das strings, e na busca o `strcmp` só é chamado quando os
  hashes de 32 bits coincidem (quase sempre apenas para a chave certa).
- **Redimensionamento sem cópias de string:** os slots são movidos para o novo
  vetor apenas copiando ponteiros; as lápides desaparecem.
- **Lápides contam no fator de carga:** evita que uma tabela cheia de lápides
  fique sem slots `VAZIO` (o que faria buscas malsucedidas percorrerem tudo).
- **Remoção que evita lápides desnecessárias:** se o slot seguinte ao
  removido está `VAZIO`, nenhuma sondagem passa por ali; o slot volta a ser
  `VAZIO` e as lápides imediatamente anteriores também são limpas.
- **Inserção reaproveita lápides:** a chave nova vai para o primeiro slot livre
  (vazio ou lápide) da sua sequência de sondagem.
- **A tabela é dona das chaves:** `th_inserir` duplica a string
  (`malloc` + `memcpy`, pois `strdup` não é C11) e `th_remover`/`th_destruir`
  liberam. O chamador pode reutilizar seu buffer livremente.
- **Falhas de memória não corrompem a tabela:** em caso de erro a função
  retorna `false` e a tabela continua válida; o vetor antigo só é liberado
  depois que o novo foi alocado com sucesso.
- **Proteção contra overflow** nos cálculos de capacidade e de fator de carga
  (feitos em inteiros: `usados * 4 > capacidade * 3`).
- `calloc` inicializa todos os slots como `VAZIO` (valor 0 do enum).

## Estrutura de pastas

```
tabela-hash-c/
├── .github/workflows/ci.yml     # integração contínua (testes com sanitizers + demo)
├── include/tabela_hash.h        # API pública (struct opaca + documentação)
├── src/tabela_hash.c            # implementação
├── src/main.c                   # demonstração: frequência de palavras
├── tests/test_tabela_hash.c     # testes unitários com assert
├── Makefile
├── LICENSE
└── README.md
```

## Como compilar, executar e testar

### Linux / macOS (com `make`)

```bash
make            # compila demo e testes em build/
make run        # executa a demonstração
make test       # executa os testes
make test SANITIZE=1   # testes com AddressSanitizer + UBSan
make clean      # remove build/
```

### Windows (MinGW-w64 / GCC, sem `make`)

```bat
mkdir build
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude src/tabela_hash.c src/main.c -o build/demo.exe
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude src/tabela_hash.c tests/test_tabela_hash.c -o build/test_tabela_hash.exe
build\demo.exe
build\test_tabela_hash.exe
```

Saída esperada dos testes:

```
Testes da tabela hash:
  [ok] teste_fnv1a
  [ok] teste_tabela_vazia
  [ok] teste_capacidade_inicial
  [ok] teste_um_elemento
  [ok] teste_atualizar
  [ok] teste_chave_copiada
  [ok] teste_chave_vazia_e_nula
  [ok] teste_crescimento
  [ok] teste_remocao_e_lapides
  [ok] teste_lapides_nao_crescem_a_tabela
  [ok] teste_percorrer
  [ok] teste_ponteiros_nulos
12 testes passaram
```

## Exemplo de saída

A demonstração conta a frequência das palavras de um trava-língua
(buscar + inserir/atualizar) e mostra os redimensionamentos. Saída real de `make run`:

```
== Tabela hash (enderecamento aberto, sondagem linear) ==
FNV-1a("hash") = 0xcec577d1

Texto: "o rato roeu a roupa do rei de roma e a rainha de raiva roeu o resto da roupa do rei"

capacidade inicial: 8
  redimensionou: 8 -> 16 (ao inserir "rei", 7 chaves)
  redimensionou: 16 -> 32 (ao inserir "resto", 13 chaves)

Frequencia das 14 palavras distintas:
  a        2
  da       1
  de       2
  do       2
  e        1
  o        2
  rainha   1
  raiva    1
  rato     1
  rei      2
  resto    1
  roeu     2
  roma     1
  roupa    2

buscar "roupa": 2
buscar "castelo": nao encontrada
atualizar "rei" -> 100 (tamanho continua 14)
remover "rato": ok
remover "rato" de novo: nao encontrada
tamanho final: 13, capacidade: 32
```

## Referências

- CORMEN, T. H.; LEISERSON, C. E.; RIVEST, R. L.; STEIN, C. *Algoritmos: Teoria e Prática*. 3. ed. Rio de Janeiro: Elsevier, 2012. (Cap. 11 — Tabelas de espalhamento; 11.4 — Endereçamento aberto.)
- SEDGEWICK, R.; WAYNE, K. *Algorithms*. 4. ed. Addison-Wesley, 2011. (Seção 3.4 — Hash Tables; *linear probing*.)
- KNUTH, D. E. *The Art of Computer Programming, Vol. 3: Sorting and Searching*. 2. ed. Addison-Wesley, 1998. (Seção 6.4 — Hashing.)
- FOWLER, G.; NOLL, L. C.; VO, K.-P. *FNV Hash*. Disponível em: <http://www.isthe.com/chongo/tech/comp/fnv/>.

## Licença

Distribuído sob a licença MIT. Veja [LICENSE](LICENSE).
