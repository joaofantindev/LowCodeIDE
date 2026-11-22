# C++ Visual IDE

IDE visual para C++ com Qt Widgets, no estilo Delphi/Lazarus. Este repositório contém a **IDE**; os projetos criados por ela são gerados em pastas separadas.

## Estado atual — F0/F1/F2 concluídos

- [x] Estrutura de projeto (`project.json`, `app.vars.json`, `forms/*.ui`, `generated/`, `src/`)
- [x] Interface em `.ui` padrão Qt, compilada com `AUTOUIC` (posicionamento livre, alças, undo/redo e Object Inspector vêm do Qt Designer)
- [x] Gerador de código: `bindings.h`, `app_vars.h/.cpp`, `CMakeLists.txt`, slots e arquivos do usuário
- [x] Variáveis de runtime (carregadas de `app.vars.json`, sem `constexpr`)
- [x] Conexões no `.ui` (o `uic` gera os `connect`); o gerador acrescenta slots novos sem apagar o código escrito pelo usuário
- [x] Editor: numeração, linha atual, syntax highlight, auto-indentação, auto-fechar brackets, Ctrl+Space
- [x] F1: clangd/LSP — autocomplete, hover, ir para definição (F2) e diagnósticos sublinhados
- [x] Árvore de projeto, salvamento, painéis Saída/Terminal, Compilar (F7), Executar (F5) e Parar (Shift+F5)
- [x] Designer nativo (paleta, drag & drop, canvas e inspetor) como alternativa ao Qt Designer
- [x] Tema escuro (menu `Exibir`)
- [ ] F3/F4: múltiplos formulários com Nova Instância/Janela e navegação entre eles

## Ambiente usado

- Qt 6.12 (MSYS2/MinGW-w64), CMake 4.5, Ninja 1.13
- Compilador: GCC 16.2 (MinGW-w64)
- clangd 23.1 (`pacman -S mingw-w64-x86_64-clang-tools-extra`) — necessário para intellisense; sem ele a IDE continua funcionando, mas sem autocomplete/diagnósticos
- Qt Designer (`pacman -S mingw-w64-x86_64-qt6-tools`) — necessário para `Projeto > Editar interface (Ctrl+D)`; o `uic` já vem com o Qt base

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
2. A IDE cria a estrutura (incluindo `forms\MainWindow.ui`), gera o código e abre os arquivos principais. A aba **Designer** aparece primeiro.
3. `Projeto > Editar interface` (Ctrl+D) abre o **Qt Designer** no `forms\<form>.ui`. Arraste componentes, positione livremente, redimensione com as alças, use o Object Inspector e crie conexões (sinal → slot) normalmente. Ao salvar no Designer, a IDE detecta a mudança, recarrega o formulário e regera o código automaticamente (undo/redo é o nativo do Qt Designer).
4. A aba **Designer** da própria IDE continua disponível: paleta, drag & drop, canvas e inspetor. Ela grava no mesmo `.ui`, com suporte a `$variavel` no inspetor.
5. `Projeto > Gerar código` (Ctrl+G) regera `generated/`, o `CMakeLists.txt` e acrescenta slots novos em `src/` sem apagar o código que você escreveu nos slots existentes.
6. `Projeto > Compilar` (F7) executa CMake + Ninja e mostra a saída no painel inferior.
7. `Projeto > Executar` (F5) inicia `build\<projeto>.exe` (compila antes, se necessário). A saída do programa aparece na aba **Terminal**; `Shift+F5` encerra.
8. Dê duplo clique num arquivo na árvore à esquerda para editá-lo; `Ctrl+S` salva.
9. No editor: `Ctrl+Space` abre o autocomplete do clangd, `F2` vai para a definição, passar o mouse mostra a documentação e erros aparecem sublinhados.
10. `Exibir > Tema escuro` alterna o tema.
11. Também é possível abrir um projeto direto: `CppVisualIDE.exe "C:\caminho\project.json"`.

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

O binding fica no `project.json` (o `.ui` guarda apenas um valor neutro). O gerador emite `generated/bindings.h` com `applyBindings(host, &ui)`, chamado logo após `ui.setupUi(this)`. Assim, alterar `app.vars.json` em runtime — ou chamar `vars().load(...)` novamente seguido de `applyBindings(this, &ui)` — reaplica todos os valores ligados às variáveis.
