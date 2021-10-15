#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#define COMP_REG 64   //tamanho do registro
#define COMP_CAMPO 60 //tamanho do campo (regsitro menos o cabeçalho)

int proximoRegistro(char registro[64], FILE *file);


struct{
    int topo_ped;
}cab; // cabeçalho do arquivo ( 4 bytes) 

int byteoffset(int RRN)
{
    return RRN * COMP_REG + sizeof(cab);
}

int lendo_campo(char campo[], int tamanho, FILE *file)
{
    int i = 0;
    char k = fgetc(file);

    //Leitura dos caracteres ate atingir o delimitador('|')
    while (k != '|')
    {
        if (feof(file)) // Caso o fim do arquivo for alcancado antes do fim do campo
            return -1;

        if (i < tamanho - 1)
            campo[i++] = k;

        k = fgetc(file);
    }

    campo[i] = 0;

    return i;
}

int lendo_linha(char campo[], int tamanho, FILE *file){
    int i = 0;
    char k = fgetc(file);

    //Leitura dos caracteres ate atingir o delimitador('\n')
    while (k != '\n')
    {
        if (feof(file)) 
            return -1;

        if (i < tamanho - 1)
            campo[i++] = k;

        k = fgetc(file);
    }

    campo[i] = 0;

    return i;

}

void importacao(char nomeArquivoImportacao[])  //  ./programa -i nome_arquivo_importacao.txt        
{
    FILE *arquivoOriginal;
    FILE *arquivoDados;
    int cabecalho = -1;

    arquivoOriginal = fopen(nomeArquivoImportacao, "r");
    arquivoDados = fopen("dados.dat", "w");

    if (arquivoOriginal == NULL || arquivoDados == NULL)
    {
        fprintf(stderr, "nao foi possivel abrir os arquivos. ABORTANDO....\n");
        exit(EXIT_FAILURE);
    }

    fwrite(&cabecalho, sizeof(int), 1, arquivoDados);

    char buffer[64];

    while (!feof(arquivoOriginal))
    {
        int sucesso = proximoRegistro(buffer, arquivoOriginal);

        if (sucesso == 1)
        {
            fwrite(buffer, sizeof(char), 64, arquivoDados);
        }
    }
    fclose(arquivoOriginal);
    fclose(arquivoDados);
}

int proximoRegistro(char registro[64], FILE *file)
{
    int contPipe = 0, tam = 0;
    char c;

    memset(registro, '\0', 64); //colocar o \0 até dar 64 

    while (contPipe < 4 && tam < 64)
    {
        c = fgetc(file);
        if (c == EOF)
        {
            return -1;
        }
        
        registro[tam] = c;

        if (c == '|')
        {
            contPipe += 1;
        }
        tam += 1;
    }

    return 1;
}

int busca(char *chave, FILE *file){


    int rrn = 0;
    char campo[COMP_CAMPO];

    fseek(file, sizeof(cab), SEEK_SET);

    while (lendo_campo(campo,7,file) > -1)
    {
        if(strcmp(campo, chave) == 0){
            fseek(file, -7, SEEK_CUR);
            return rrn;
        }

        fseek(file, COMP_REG -7, SEEK_CUR);
        rrn++;
    }

    return -1;
    
}

int PED(FILE *file, int RRN){
    fseek (file, byteoffset(RRN), SEEK_SET);

    if (fgetc(file) != '*'){
        printf("\nERROR na PED");
        // exit(1);
    }

    fread(&RRN, sizeof(int), 1, file);
    fseek(file, -1 * (long)(sizeof(char) + sizeof(int)), SEEK_CUR);
    return RRN;

}

void operacoes(char *argv)  // ./programa -e nome_do_arquivo.txt     para realizar as operações abaixo
{

    FILE *dados;
    FILE *operacoesFile;

    char campo [COMP_CAMPO];       
    char buffer [COMP_REG];
    char def_op;  //definição de qual operação vai ser feita
    char chave[7];
    int RRN; 

    operacoesFile = fopen(argv, "rb");
    dados = fopen("dados.dat", "r+b");

    while ((def_op = fgetc(operacoesFile)) != EOF)
    {
        buffer[0]=0;
        fgetc(operacoesFile);

        switch(def_op){

        case 'b':                                           // b é uma operação de busca 
        {
           
           
            lendo_linha(chave, 7, operacoesFile);

            RRN = busca(chave, dados);  

            printf ("\nREGISTRO DE CHAVE %s\n", chave);

            if(RRN == -1){
                printf("REGISTRO NAO ENCONTRADO\n");
                break;
                
            }

            for (int i = 0; i < 4; i++){
                lendo_campo(campo, COMP_CAMPO, dados); 
                strcat(buffer, campo);
                strcat(buffer,"|");
            }
            
            printf("%s; RRN = %d, byte-offset %d\n",buffer, RRN, byteoffset(RRN));
            break;        
        }
     
        case 'i':                                           // i é uma operação de inserção
            
            lendo_linha(buffer, COMP_REG, operacoesFile);
            strncpy(chave, buffer, 6);                                 // mesma função do strcpy, mas esse da para escolher a quantidade de caracteres da string origem
            printf("\nINSERCAO DO REGISTRO DE CHAVE %s", chave);

            if (busca (chave, dados) != -1){                                //significa que já existe uma chave naquela posição do FILE dados

                printf("ERRO: CHAVE %s JA EXISTE NO REGISTRO", chave);
                break;
            }

            rewind(dados);
            fread(&cab, sizeof(cab), 1, dados);
           
            RRN = cab.topo_ped;

            if (RRN == -1){

                fseek(dados, 0, SEEK_END);
                fwrite(buffer, sizeof(char), COMP_REG, dados);
                printf("\nLOCAL DE INSERCAO: FIM DO ARQUIVO\n");
            }

            else{

                cab.topo_ped = PED(dados, RRN);
                
                fwrite(buffer, sizeof(char), COMP_REG, dados);
                rewind(dados);
                fwrite(&cab, sizeof(cab), 1, dados);

                printf("\nLOCAL: RRN = %d (byte-offset %d) [reutilizado]\n", RRN, byteoffset(RRN));
            }
            
            break;
            

        case 'r':                                           // r é uma operação de remoção
        
            lendo_linha(chave, 7, operacoesFile);
            printf("\nREMOCAO DO REGISTRO %s\n", chave);
            rewind(dados);

            fread(&cab, sizeof(cab), 1, dados);

            RRN = busca(chave, dados);
            
            if (RRN == -1)
            {
                puts("Erro: registro nao encontrado!");
                break;
            }
            
            fputc('*', dados);
            fwrite(&cab.topo_ped, sizeof(int), 1, dados);

            cab.topo_ped = RRN;

            rewind(dados);
            fwrite(&cab, sizeof(cab), 1, dados);

            printf("REMOVIDO");
            printf("\n");
            printf("Posicao: RRN = %d (byte-offset %d)\n", RRN, byteoffset(RRN));
            break;

            default:
            printf("\n NAO FOI POSSIVEL REMOVER");
        

            fseek(dados, 0, SEEK_END);
            if (((ftell(dados) - sizeof(cab)) % COMP_REG) != 0) /* ftell = Retorna o valor atual do indicador de posição do fluxo. 
                                                                Esse valor pode ser usado pela função fseek com origem SEEK_SET 
                                                                para retornar o indicador a posição atual.*/
            {
              printf("NAO FOI POSSIVEL REALIZAR A OPERACAO");
              exit(1);
            }
        
        
        }
    }

    fclose(operacoesFile);
    fclose(dados);
    return 0;
    
}

// vai imprimir os valores na PED que estão livres 
int imprime_ped(){                                          // ./programa -p 

    FILE *dados;
    dados = fopen("dados.dat", "rb");
    int k;
    
    rewind (dados);

    int RRN;

    if (dados == NULL)
    {
        printf("ERRO");
        exit(1);
    }

    fread(&cab, sizeof(cab), 1, dados);
    RRN = cab.topo_ped;

    printf("PED: %d", RRN);

    while (RRN > -1)
    {
        RRN = PED(dados, RRN);
        printf(" = %d", RRN);
        k++;
    }
    
    fclose(dados);
    return 0;

    
}

int main(int argc, char *argv[])
{
    

    if (argc == 3 && strcmp(argv[1], "-i") == 0)
    {
        printf("\nNome do arquivo em que ocorrera a importacao = %s\n", argv[2]);
        printf("\n");
        importacao(argv[2]);
    }
    else if (argc == 3 && strcmp(argv[1], "-e") == 0)
    {

        printf("\nModo de execucao de operacoes ativado ... nome do arquivo = %s\n", argv[2]);
        operacoes(argv[2]);
    }
    else if (argc == 2 && strcmp(argv[1], "-p") == 0)
    {
        printf("\nModo de impressao da PED ativado ...\n");
        imprime_ped();
    }
    else
    {

        fprintf(stderr, "Argumentos incorretos!\n");
        fprintf(stderr, "Modo de uso:\n");
        fprintf(stderr, "$ %s (-i|-e) nome_arquivo\n", argv[0]);
        fprintf(stderr, "$ %s -p\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return 0;
}