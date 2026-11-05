#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tabela_hash.h"

static int testes_passaram = 0;

#define RODAR(teste)                          \
    do {                                      \
        teste();                              \
        testes_passaram++;                    \
        printf("  [ok] %s\n", #teste);        \
    } while (0)

/* Gera a chave "chave-<i>" em buf. */
static const char *gerar_chave(char *buf, size_t tam, int i)
{
    snprintf(buf, tam, "chave-%d", i);
    return buf;
}

static void teste_fnv1a(void)
{
    /* Vetores de teste oficiais do FNV-1a de 32 bits. */
    assert(th_fnv1a("") == 0x811c9dc5u);
    assert(th_fnv1a("a") == 0xe40c292cu);
    assert(th_fnv1a("foobar") == 0xbf9cf968u);
}

static void teste_tabela_vazia(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    assert(th_tamanho(t) == 0);
    assert(th_capacidade(t) == 8);
    int v = 7;
    assert(!th_buscar(t, "x", &v));
    assert(v == 7);
    assert(!th_contem(t, ""));
    assert(!th_remover(t, "x"));
    th_destruir(t);
}

static void teste_capacidade_inicial(void)
{
    TabelaHash *t = th_criar(100); /* 100 / 0,75 = 133.3 -> 256 */
    assert(t != NULL);
    assert(th_capacidade(t) == 256);
    char buf[32];
    for (int i = 0; i < 100; i++) {
        assert(th_inserir(t, gerar_chave(buf, sizeof buf, i), i));
    }
    assert(th_capacidade(t) == 256); /* nao precisou redimensionar */
    th_destruir(t);
}

static void teste_um_elemento(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    assert(th_inserir(t, "um", 1));
    assert(th_tamanho(t) == 1);
    int v = 0;
    assert(th_buscar(t, "um", &v) && v == 1);
    assert(!th_contem(t, "dois"));
    assert(th_remover(t, "um"));
    assert(th_tamanho(t) == 0);
    assert(!th_contem(t, "um"));
    th_destruir(t);
}

static void teste_atualizar(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    assert(th_inserir(t, "k", 1));
    assert(th_inserir(t, "k", 2));
    assert(th_inserir(t, "k", -3));
    assert(th_tamanho(t) == 1);
    int v;
    assert(th_buscar(t, "k", &v) && v == -3);
    th_destruir(t);
}

static void teste_chave_copiada(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    char buf[16];
    strcpy(buf, "original");
    assert(th_inserir(t, buf, 10));
    strcpy(buf, "alterada"); /* o buffer do chamador muda... */
    assert(th_contem(t, "original")); /* ...mas a tabela guardou uma copia */
    assert(!th_contem(t, "alterada"));
    th_destruir(t);
}

static void teste_chave_vazia_e_nula(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    assert(th_inserir(t, "", 5));
    int v;
    assert(th_buscar(t, "", &v) && v == 5);
    assert(!th_inserir(t, NULL, 1));
    assert(!th_buscar(t, NULL, &v));
    assert(!th_remover(t, NULL));
    assert(th_tamanho(t) == 1);
    th_destruir(t);
}

static void teste_crescimento(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    char buf[32];
    const int n = 50000;
    for (int i = 0; i < n; i++) {
        assert(th_inserir(t, gerar_chave(buf, sizeof buf, i), i));
        /* invariante: fator de carga <= 0,75 */
        assert(th_tamanho(t) * 4 <= th_capacidade(t) * 3);
    }
    assert(th_tamanho(t) == (size_t)n);
    size_t cap = th_capacidade(t);
    assert((cap & (cap - 1)) == 0); /* potencia de 2 */

    int v;
    for (int i = 0; i < n; i++) {
        assert(th_buscar(t, gerar_chave(buf, sizeof buf, i), &v) && v == i);
    }
    assert(!th_contem(t, "chave-50000"));
    th_destruir(t);
}

static void teste_remocao_e_lapides(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    char buf[32];
    const int n = 1000;
    for (int i = 0; i < n; i++) {
        assert(th_inserir(t, gerar_chave(buf, sizeof buf, i), i));
    }
    /* Remove os pares: as chaves impares devem continuar acessiveis,
     * mesmo que suas sondagens passem por lapides. */
    for (int i = 0; i < n; i += 2) {
        assert(th_remover(t, gerar_chave(buf, sizeof buf, i)));
    }
    assert(th_tamanho(t) == (size_t)n / 2);
    int v;
    for (int i = 0; i < n; i++) {
        bool achou = th_buscar(t, gerar_chave(buf, sizeof buf, i), &v);
        assert(achou == (i % 2 == 1));
        assert(!achou || v == i);
    }
    /* Remover de novo falha; reinserir reaproveita os slots. */
    assert(!th_remover(t, "chave-0"));
    for (int i = 0; i < n; i += 2) {
        assert(th_inserir(t, gerar_chave(buf, sizeof buf, i), -i));
    }
    assert(th_tamanho(t) == (size_t)n);
    assert(th_buscar(t, "chave-10", &v) && v == -10);
    assert(th_buscar(t, "chave-11", &v) && v == 11);
    th_destruir(t);
}

static void teste_lapides_nao_crescem_a_tabela(void)
{
    /* Muitas insercoes e remocoes com poucas chaves vivas: as lapides sao
     * limpas por reconstrucao e a capacidade nao deve explodir. */
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    char buf[32];
    for (int i = 0; i < 100000; i++) {
        assert(th_inserir(t, gerar_chave(buf, sizeof buf, i), i));
        if (i >= 4) {
            assert(th_remover(t, gerar_chave(buf, sizeof buf, i - 4)));
        }
    }
    assert(th_tamanho(t) == 4);
    assert(th_capacidade(t) <= 16);
    for (int i = 99996; i < 100000; i++) {
        assert(th_contem(t, gerar_chave(buf, sizeof buf, i)));
    }
    th_destruir(t);
}

static void somar_valores(const char *chave, int valor, void *contexto)
{
    (void)chave;
    *(long *)contexto += valor;
}

static void teste_percorrer(void)
{
    TabelaHash *t = th_criar(0);
    assert(t != NULL);
    char buf[32];
    long esperado = 0;
    for (int i = 1; i <= 100; i++) {
        assert(th_inserir(t, gerar_chave(buf, sizeof buf, i), i));
        esperado += i;
    }
    assert(th_remover(t, "chave-100"));
    esperado -= 100;
    long soma = 0;
    th_percorrer(t, somar_valores, &soma);
    assert(soma == esperado);
    th_destruir(t);
}

static void teste_ponteiros_nulos(void)
{
    assert(!th_inserir(NULL, "a", 1));
    assert(!th_buscar(NULL, "a", NULL));
    assert(!th_contem(NULL, "a"));
    assert(!th_remover(NULL, "a"));
    assert(th_tamanho(NULL) == 0);
    assert(th_capacidade(NULL) == 0);
    th_percorrer(NULL, somar_valores, NULL);
    th_destruir(NULL);
}

int main(void)
{
    puts("Testes da tabela hash:");
    RODAR(teste_fnv1a);
    RODAR(teste_tabela_vazia);
    RODAR(teste_capacidade_inicial);
    RODAR(teste_um_elemento);
    RODAR(teste_atualizar);
    RODAR(teste_chave_copiada);
    RODAR(teste_chave_vazia_e_nula);
    RODAR(teste_crescimento);
    RODAR(teste_remocao_e_lapides);
    RODAR(teste_lapides_nao_crescem_a_tabela);
    RODAR(teste_percorrer);
    RODAR(teste_ponteiros_nulos);
    printf("%d testes passaram\n", testes_passaram);
    return 0;
}
