# Investigação de Compressão UE3 - Frontend.umap

## Resumo

O arquivo `Frontend.umap` do Batman Arkham Asylum está compactado com LZO (método 2).
A estrutura do arquivo é mais complexa que o esperado.

## Estrutura do Header UE3

```
Offset  Size  Field
0       4     Signature (0x9E2A83C1)
4       2     Version (576)
6       2     Licensee (21)
8       4     HeaderSize (93096)
12      4     FolderName length (5)
13      5     FolderName ("None\0")
18      4     PackageFlags (0x028A000D)
22      4     NameCount (910)
26      4     NameOffset (0x1E4 = 484)
30      4     ExportCount (843)
34      4     ExportOffset (0x7E90 = 32400)
38      4     ImportCount (227)
42      4     ImportOffset (0x65BC = 26044)
46      4     DependsOffset (0x15E7C = 89724)
50      16    GUID
66      4     GenerationCount
70+     N     GenerationInfo (8 bytes each)
...     4     EngineVersion
...     4     CookerVersion
82      4     CompressionFlags (0x00020057)
86      4     ChunkCount (2)
90      16    Chunk 0 info
106     16    Chunk 1 info
122     8     Padding
130     8     Extra padding
137     -     Fim do header parsing
```

## Chunks de Compressão

O header principal contém 2 chunks:
- Chunk 0: UncompressedOffset=14, UncompressedSize=484, CompressedSize=708
- Chunk 1: UncompressedOffset=509832, UncompressedSize=1049020, CompressedSize=510540

**Importante:** O CompressedOffset nos chunks do header principal NÃO é um offset de arquivo válido.
Os chunk headers reais estão em offsets iguais aos valores de CompressedSize:
- Chunk header 0 em offset 708 (0x2C4)
- Chunk header 1 em offset 510540 (0x7CA4C)

## Formato dos Chunk Headers

Cada chunk header tem 16 bytes:
```
Offset  Size  Field
0       4     Tag (0x9E2A83C1)
4       4     BlockSize (131072 = 128KB)
8       4     TotalCompressedSize
12      4     TotalUncompressedSize
```

Seguido por blocos de 8 bytes:
```
Offset  Size  Field
0       4     BlockCompressedSize
4       4     BlockUncompressedSize
```

## Status da Implementação

### ✅ Descoberto e Implementado
- Estrutura completa do header UE3
- Localização dos chunk headers
- Formato dos blocos comprimidos
- Runtime HelenGameHook com suporte a graphics options

### ⚠️ Problema Pendente
A descompressão LZO1x-1 não está funcionando com MiniLZO (porta C#):
- MiniLZO causa AccessViolationException
- O problema pode estar na implementação C# do MiniLZO ou no formato específico dos dados UE3

### 🔧 Soluções Possíveis
1. **Usar UModel/UE Viewer** para extrair Frontend.umap descomprimido
2. **Implementar LZO1x-1** em C# seguro (sem unsafe code)
3. **Usar biblioteca nativa LZO** via P/Invoke
4. **Implementar decompressor LZO1x** do zero baseado na spec oficial

## Status do Runtime

O runtime HelenGameHook já suporta graphics options via:
- Step kind `load-batman-graphics-draft-into-config`
- Step kind `apply-batman-graphics-config`
- Bindings get-int/set-int para 15 config keys

O que falta é apenas o delta do Frontend.umap para injetar o menu visual de graphics options.

## Próximos Passos

1. Resolver decompressão LZO (ver soluções acima)
2. Gerar delta do Frontend.umap
3. Testar menu de graphics options no Batman
