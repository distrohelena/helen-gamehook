# Batman Graphics Options Testing

Este script automatiza a captura de telas do Batman Arkham Asylum até detectar o menu **Graphics Options** usando o sistema de reconhecimento de telas do HelenUI.

## Pré-requisitos

1. **Batman Arkham Asylum GOTY** instalado
2. **.NET 8.0 SDK** instalado
3. **Windows 10/11** (para OCR nativo) ou **Tesseract OCR** instalado

## Configuração do OCR

O recognition CLI suporta dois motores de OCR:

### Opção 1: Windows Native OCR (Recomendado)
- Disponível nativamente no Windows 10/11
- Não requer instalação adicional
- Configuração automática

### Opção 2: Tesseract OCR
- Instale o Tesseract: https://github.com/UB-Mannheim/tesseract/wiki
- Atualize o arquivo `recognition-config.sample.json` em `C:\dev\helenui\plugins\recognition-cli\`:

```json
{
  "ocr": {
    "engines": [
      { "type": "tesseract", "exePath": "C:\\Program Files\\Tesseract-OCR\\tesseract.exe" },
      { "type": "tesseract", "exePath": "C:\\Program Files\\Tesseract-OCR\\tesseract.exe" }
    ]
  }
}
```

## Uso

### Uso Básico

```powershell
cd C:\dev\helenhook\games\HelenBatmanAA\scripts
.\Test-BatmanGraphicsOptionsMenu.ps1
```

O script vai:
1. Iniciar o Batman Arkham Asylum
2. Capturar screenshots a cada 2 segundos
3. Analisar cada screenshot com o recognition CLI
4. Parar quando detectar o menu Graphics Options

### Uso Avançado

```powershell
.\Test-BatmanGraphicsOptionsMenu.ps1 `
  -GamePath "C:\Path\To\ShippingPC-BmGame.exe" `
  -OutputDir "C:\My\Screenshots" `
  -IntervalSeconds 3 `
  -MaxScreenshots 50
```

### Parâmetros

| Parâmetro | Padrão | Descrição |
|-----------|--------|-----------|
| `-GamePath` | Steam GOTY default | Caminho para o executável do jogo |
| `-OutputDir` | `..\..\artifacts\graphics-options-screenshots` | Diretório de output |
| `-IntervalSeconds` | `2` | Intervalo entre capturas |
| `-MaxScreenshots` | `100` | Máximo de capturas antes de desistir |

## Como Funciona

O script usa o sistema de reconhecimento do HelenUI que:

1. **Define telas no `batman-aa.json`**:
   - Title → Saves → Main → Options → GraphicsOptions
   - Cada tela tem regras de reconhecimento baseadas em texto

2. **GraphicsOptions** é detectado quando:
   - Texto "Graphics" aparece na tela (peso: 1.0)
   - E pelo menos um destes aparece: "Brightness", "Gamma", "VSync" (peso: 0.75)

3. **Fluxo de navegação esperado**:
   ```
   Title Screen → Saved Game Select → Main Menu → Options → Graphics
   ```

## Output

O script salva:
- Todas as screenshots capturadas em `OutputDir`
- Nomeadas como `screenshot_0001.png`, `screenshot_0002.png`, etc.
- Quando detecta GraphicsOptions, mostra evidências do match

## Troubleshooting

### "Windows native OCR failed"
- Verifique se está no Windows 10/11
- Ou instale o Tesseract e configure conforme acima

### "Game executable not found"
- Use o parâmetro `-GamePath` para especificar o caminho correto

### Recognition não detecta nenhuma tela
- Certifique-se de que o jogo está em foco e visível
- Verifique se a resolução está adequada para OCR
- Tente aumentar o `-IntervalSeconds` para dar mais tempo

## Arquivos Relacionados

- `batman-aa.json` (C:\dev\helenui\): Definição das telas e transições
- `pack.json`: Configuração do pack com graphics options
- `recognition-config.sample.json`: Configuração do OCR
