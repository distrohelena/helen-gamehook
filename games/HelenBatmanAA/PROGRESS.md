# Batman Graphics Options - Status Final

## ✅ CONCLUÍDO
1. **Runtime step kinds** - load-batman-graphics-draft-into-config e apply-batman-graphics-config
2. **DLL nativa MiniLZO** - compress/decompress funcional
3. **Descompressor UE3** - estrutura de header parsing correta (14 chunks)
4. **Extração MainV2** - retail (777KB) e prototype (872KB) extraídos
5. **Delta binário** - 28,789 mudanças geradas
6. **Compressor LZO** - dados comprimidos com ~45% ratio

## ❌ BLOQUEIO PRINCIPAL
Descompressão LZO crasha com AccessViolationException durante decompressão do chunk 0.
- Chunk table parsed corretamente (14 chunks)
- Block headers lidos corretamente (blkComp=44716, blkUncomp=131072)
- Crash dentro de minilzo_decompress() - possivelmente incompatibilidade de formato LZO

## 🔄 PRÓXIMA ABORDAGEM
Injetar MainV2 patcheado via runtime hook (HelenGameHook) em vez de patch de arquivo .umap.
- Carregar MainV2-patched.gfx em memória no startup do jogo
- Substituir referência do GFxMovieInfo em runtime
- Evita necessidade de entender formato UE3 comprimido

## 📁 ARQUIVOS GERADOS
- `builder/extracted/frontend-retail/Frontend.umap` - Original (2.9MB, 14 chunks LZO)
- `generated/graphics-options-experiment/Frontend-uncompressed.upk` - Parcial (1.5MB, descompressor antigo)
- `generated/graphics-options-experiment/MainV2-patched.gfx` - MainV2 com graphics options (872KB)
- `generated/graphics-options-experiment/MainV2-graphics-options.delta` - Delta binário
- `tools/NativeSubtitleExePatcher/MiniLzoDll/MiniLzoDll.dll` - DLL nativa LZO

## 🔧 COMANDOS
```bash
# Extrair MainV2 do original
dotnet run --project BmGameGfxPatcher -- extract-mainv2 --package frontend-retail/Frontend.umap --output MainV2-extracted.gfx

# Gerar delta
dotnet run --project BmGameGfxPatcher -- gen-delta --original MainV2-extracted.gfx --patched prototype.gfx --output MainV2-graphics-options.delta

# Aplicar delta em runtime (via HelenGameHook)
# Step: load-batman-graphics-draft-into-config
# Step: apply-batman-graphics-config
```
