# C++ Visual IDE

IDE visual para C++ com Qt Widgets, no estilo Delphi/Lazarus. Este repositório contém a **IDE**; os projetos criados por ela são gerados em pastas separadas.

## Estado atual — F0 (fundação) concluída

- [x] Estrutura de projeto (`project.json`, `app.vars.json`, `forms/`, `generated/`, `src/`)
- [x] Gerador de código: `ui_*.h`, `app_vars.h/.cpp`, `CMakeLists.txt`, arquivos do usuário
- [x] Variáveis de runtime (carregadas de `app.vars.json`, sem `constexpr`)
- [x] Eventos gerados em região delimitada por marcadores (código do usuário preservado)
- [x] Editor com numeração de linhas, destaque de linha atual e syntax highlight
- [x] Árvore de projeto, salvamento, painel de saída, Compilar (F7) e Executar (F5)
- [x] F2 (parcial): designer visual com paleta, drag & drop, canvas e inspetor de propriedades
- [ ] F1: intellisense com clangd/LSP (autocomplete, hover, ir para definição, diagnósticos)
- [ ] F3/F4: refinamento do build, undo/redo, múltiplos forms, temas

## Ambiente usado

- Qt 6.12 (MSYS2/MinGW-w64), CMake 4.5, Ninja 1.13
- Compilador: GCC 16.2 (MinGW-w64)

## Como compilar a IDE

No **MSYS2 MINGW64 Shell**:

```bash
cd /c/Users/joaof/Desktop/code/cpp/IDE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/CppVisualIDE.exe
```

Se preferir PowerShell/cmd, o `cmake` e o `g++` usados estão em `C:\msys64\mingw64\bin`. Após a compilação, o `windeployqt6` copia automaticamente os DLLs do Qt e o plugin `platforms/qwindows.dll` para a pasta do executável, então o `.exe` roda sem precisar configurar PATH. O mesmo vale para os projetos gerados pela IDE.

## Instalador (interface gráfica com barra de progresso)

Requer NSIS 3 (`winget install -e --id NSIS.NSIS`), já instalado nesta máquina.

```powershell
powershell -ExecutionPolicy Bypass -File installer\build-installer.ps1
```

Gera `installer\CppVisualIDE-Setup.exe`: assistente em português, páginas de boas-vindas/componentes/diretório/instalação com barra de progresso e opção de abrir a IDE ao final. Instala por padrão em `%LOCALAPPDATA%\Programs\CppVisualIDE` (sem precisar de administrador), cria atalhos e inclui desinstalador.

Instalação silenciosa (útil para teste):

```powershell
Start-Process .\installer\CppVisualIDE-Setup.exe -ArgumentList '/S', '/D=C:\Temp\CppVisualIDE' -Wait
```

## Dependências

Auditadas e atualizadas com `pacman -Syu` (MSYS2). Deploy automático pós-build:

- `windeployqt6` — DLLs do Qt + plugins (`platforms/qwindows.dll`, `styles`, `imageformats`, etc.)
- `scripts/deploy-runtime.ps1` — dependências do MinGW detectadas via `ldd` (`libstdc++`, `libgcc_s_seh`, `libicu*`, `libfreetype`, `libharfbuzz`, `libglib`, `zlib`, etc.)

O mesmo par de scripts é gerado dentro de cada projeto criado pela IDE (`tools/deploy-runtime.ps1`), então os `.exe` dos projetos também ficam independentes do PATH do MSYS2.

## Como usar

1. `Arquivo > Novo Projeto...` — escolha o nome e a pasta pai.
2. A IDE cria a estrutura, gera o código e abre os arquivos principais. A aba **Designer** aparece primeiro.
3. No **Designer**: arraste um componente da paleta para o canvas (ou dê duplo clique nele). Clique no componente para selecioná-lo, edite propriedades no inspetor à direita e use `+ Evento clicked` para criar o slot. `Delete` remove o selecionado. Valores começando com `$` viram binding de runtime (ex.: `$alturaBotao`).
4. `Projeto > Gerar código` (Ctrl+G) regera apenas os arquivos em `generated/` e os arquivos de configuração. Os arquivos em `src/` **não** são sobrescritos depois da criação.
5. `Projeto > Compilar` (F7) executa CMake + Ninja e mostra a saída no painel inferior.
6. `Projeto > Executar` (F5) inicia `build\<projeto>.exe` (compila antes, se necessário).
7. Dê duplo clique num arquivo na árvore à esquerda para editá-lo; `Ctrl+S` salva.
8. Também é possível abrir um projeto direto: `CppVisualIDE.exe "C:\caminho\project.json"`.

## Variáveis em runtime

`app.vars.json` é copiado para junto do executável na compilação:

```json
{
  "alturaBotao": 32,
  "corPrimaria": "#2d7d9a",
  "titulo": "MeuProjeto"
}
```

No formulário, uma propriedade pode referenciar uma variável:

```json
"minimumHeight": { "$var": "alturaBotao" }
```

O gerador emite `btnOk->setMinimumHeight(vars().alturaBotao);` e também replica essas atribuições em `Ui::MainWindow::applyBindings()`, que pode ser chamada após alterar as variáveis em runtime.
