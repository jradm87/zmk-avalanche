# ZMK Avalanche Firmware

Firmware ZMK para o teclado split Avalanche com nice!nano v2.

## Estrutura do Projeto

```
zmk-avalanche/
├── boards/shields/avalanche/   # Definição do shield
├── config/
│   ├── avalanche.conf          # Configurações globais
│   └── avalanche.keymap        # Mapeamento de teclas
├── firmware/                   # Arquivos .uf2 gerados
├── build.sh                    # Script de build local
└── build.yaml                  # Configuração GitHub Actions
```

## Modos de Operação

### Modo Standalone (Padrão)
- **Left** = Central (conecta ao PC via USB ou BLE)
- **Right** = Periférico (conecta ao Left via BLE)

### Modo Dongle
- **Dongle** = Central (conecta ao PC via USB)
- **Left** = Periférico (conecta ao Dongle via BLE)
- **Right** = Periférico (conecta ao Dongle via BLE)

**Vantagens do Dongle:**
- Melhor vida de bateria nos dois lados
- Conexão mais estável
- Menor latência média

---

## Build Local

### Pré-requisitos

```bash
# Instalar dependências do sistema
sudo apt install ninja-build device-tree-compiler python3-pip python3-venv

# Baixar e instalar Zephyr SDK
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
tar xf zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
cd zephyr-sdk-0.16.8
./setup.sh -t arm-zephyr-eabi -c
cd ..

# Criar ambiente virtual e instalar west
python3 -m venv .venv
source .venv/bin/activate
pip install west pyelftools

# Inicializar e atualizar dependências
west init -l config
west update
west zephyr-export
```

### Comandos de Build

```bash
# Build modo standalone (left como central)
./build.sh standalone
# ou simplesmente
./build.sh

# Build modo dongle (dongle como central)
./build.sh dongle
```

### Arquivos Gerados

**Standalone:**
```
firmware/
├── avalanche_left.uf2      # Left (central)
├── avalanche_right.uf2     # Right (periférico)
└── settings_reset.uf2      # Reset de configurações
```

**Dongle:**
```
firmware/
├── avalanche_dongle.uf2        # Dongle (central)
├── avalanche_left_dongle.uf2   # Left (periférico)
├── avalanche_right_dongle.uf2  # Right (periférico)
└── settings_reset.uf2          # Reset de configurações
```

---

## Flash do Firmware

### Como entrar no modo Bootloader

1. Conecte o nice!nano via USB
2. Dê **double-tap** no botão reset
3. Um drive USB chamado `NICENANO` vai aparecer
4. Copie o arquivo `.uf2` para o drive
5. O dispositivo reinicia automaticamente

### Flash Standalone (primeira vez)

```bash
# 1. Flash reset no left
# Copie settings_reset.uf2 para o left

# 2. Flash reset no right  
# Copie settings_reset.uf2 para o right

# 3. Flash firmware no left
# Copie avalanche_left.uf2 para o left

# 4. Flash firmware no right
# Copie avalanche_right.uf2 para o right
```

### Flash Dongle (primeira vez)

⚠️ **IMPORTANTE**: Flash `settings_reset.uf2` em **TODOS** os dispositivos primeiro!

```bash
# 1. Flash reset em todos (dongle, left, right)
# Copie settings_reset.uf2 para cada um

# 2. Flash firmware do dongle
# Copie avalanche_dongle.uf2 para o dongle

# 3. Flash firmware do left
# Copie avalanche_left_dongle.uf2 para o left

# 4. Flash firmware do right
# Copie avalanche_right_dongle.uf2 para o right
```

Os periféricos vão parear automaticamente com o dongle/central.

### Atualizando apenas o Keymap

Para mudanças no keymap, geralmente basta flashar o **central**:
- Standalone: flash apenas o `avalanche_left.uf2`
- Dongle: flash apenas o `avalanche_dongle.uf2`

---

## Build via GitHub Actions

O build também acontece automaticamente via GitHub Actions quando você faz push.

1. Faça suas alterações no keymap/config
2. Commit e push:
   ```bash
   git add -A
   git commit -m "Atualizar keymap"
   git push
   ```
3. Vá em **Actions** no GitHub
4. Baixe os artifacts com os arquivos `.uf2`

---

## Customização

### Keymap

Edite `config/avalanche.keymap` para modificar as teclas.

### Configurações

Edite `config/avalanche.conf` para modificar:
- Display OLED
- RGB underglow
- Encoder
- Bluetooth

---

## Troubleshooting

### Lados não pareiam

1. Flash `settings_reset.uf2` em **ambos** os lados
2. Flash os firmwares novamente
3. Ligue ambos ao mesmo tempo

### Dongle não conecta

1. Flash `settings_reset.uf2` em **todos** (dongle + left + right)
2. Flash os firmwares na ordem: dongle → left → right
3. Conecte o dongle via USB e ligue os lados

### Reset de fábrica

Flash `settings_reset.uf2` no dispositivo para limpar todas as configurações de pareamento.

---

## Hardware

- **MCU**: nice!nano v2 (nRF52840)
- **Display**: OLED SSD1306 128x64 (opcional)
- **Encoder**: EC11 (opcional)
- **RGB**: WS2812 (opcional)

### Para modo Dongle

- 1x nice!nano v2 adicional
- Cabo USB para conectar ao PC
- (Opcional) Case pequeno para o dongle

---

## Links

- [ZMK Documentation](https://zmk.dev/docs)
- [ZMK Keymap Editor](https://nickcoutsos.github.io/keymap-editor/)
- [nice!nano Documentation](https://nicekeyboards.com/docs/nice-nano/)
