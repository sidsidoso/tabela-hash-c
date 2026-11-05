#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tabela_hash.h"

/* Par copiado da tabela para impressao em ordem alfabetica. */
typedef struct {
    const char *chave;
    int valor;
} Par;

typedef struct {
    Par *pares;
    size_t n;
} Coleta;

static void coletar(const char *chave, int valor, void *contexto)
{
    Coleta *c = contexto;
    c->pares[c->n].chave = chave;
    c->pares[c->n].valor = valor;
    c->n++;
}

static int comparar_pares(const void *a, const void *b)
{
    return strcmp(((const Par *)a)->chave, ((const Par *)b)->chave);
}

/* Imprime os pares em ordem alfabetica (a tabela em si nao tem ordem). */
static bool imprimir_ordenado(const TabelaHash *t)
{
    size_t n = th_tamanho(t);
    Coleta c = {malloc((n > 0 ? n : 1) * sizeof(Par)), 0};
    if (c.pares == NULL) {
        return false;
    }
    th_percorrer(t, coletar, &c);
    qsort(c.pares, c.n, sizeof(Par), comparar_pares);
    for (size_t i = 0; i < c.n; i++) {
        printf("  %-8s %d\n", c.pares[i].chave, c.pares[i].valor);
    }
    free(c.pares);
    return true;
}

int main(void)
{
    TabelaHash *t = th_criar(0);
    if (t == NULL) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        return EXIT_FAILURE;
    }

    puts("== Tabela hash (enderecamento aberto, sondagem linear) ==");
    printf("FNV-1a(\"hash\") = 0x%08lx\n\n", (unsigned long)th_fnv1a("hash"));

    /* Contagem de frequencia de palavras: buscar + inserir/atualizar. */
    char texto[] = "o rato roeu a roupa do rei de roma e a rainha "
                   "de raiva roeu o resto da roupa do rei";
    printf("Texto: \"%s\"\n\n", texto);

    size_t cap_anterior = th_capacidade(t);
    printf("capacidade inicial: %zu\n", cap_anterior);
    for (char *p = strtok(texto, " "); p != NULL; p = strtok(NULL, " ")) {
        int contagem = 0;
        th_buscar(t, p, &contagem);
        if (!th_inserir(t, p, contagem + 1)) {
            fprintf(stderr, "erro: memoria insuficiente\n");
            th_destruir(t);
            return EXIT_FAILURE;
        }
        if (th_capacidade(t) != cap_anterior) {
            printf("  redimensionou: %zu -> %zu (ao inserir \"%s\", %zu chaves)\n",
                   cap_anterior, th_capacidade(t), p, th_tamanho(t));
            cap_anterior = th_capacidade(t);
        }
    }

    printf("\nFrequencia das %zu palavras distintas:\n", th_tamanho(t));
    if (!imprimir_ordenado(t)) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        th_destruir(t);
        return EXIT_FAILURE;
    }

    int v;
    if (th_buscar(t, "roupa", &v)) {
        printf("\nbuscar \"roupa\": %d\n", v);
    }
    printf("buscar \"castelo\": %s\n", th_contem(t, "castelo") ? "encontrada" : "nao encontrada");

    th_inserir(t, "rei", 100);
    th_buscar(t, "rei", &v);
    printf("atualizar \"rei\" -> %d (tamanho continua %zu)\n", v, th_tamanho(t));

    printf("remover \"rato\": %s\n", th_remover(t, "rato") ? "ok" : "nao encontrada");
    printf("remover \"rato\" de novo: %s\n", th_remover(t, "rato") ? "ok" : "nao encontrada");
    printf("tamanho final: %zu, capacidade: %zu\n", th_tamanho(t), th_capacidade(t));

    th_destruir(t);
    return EXIT_SUCCESS;
}
