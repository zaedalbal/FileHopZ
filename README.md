# FileHopZ

FileHopZ - это легковесный инструмент для передачи файлов по сети, ориентированный на простоту использования и скорость передачи

## Требования

* Компилятор с поддержкой **C++23**
* **CMake 3.25+**
* **Git**
* **Boost**
* **CLI11**

### Установка зависимостей

**Для Ubuntu/Debian:**

```bash
sudo apt install git cmake g++ libboost-all-dev libssl-dev libspdlog-dev libcli11-dev
```

**Для Fedora:**

```bash
sudo dnf install git cmake gcc-c++ boost-devel openssl-devel spdlog-devel cli11-devel
```

**Для Arch Linux / Manjaro:**

```bash
sudo pacman -S git cmake base-devel boost openssl spdlog cli11
```

(если ваша версия CMake < 3.25, вы можете попробовать поменять CMakeLists.txt)

## Сборка

```bash
mkdir build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ..

cmake --build .
```

После успешной сборки в каталоге `build` будет создан исполняемый файл

## Установка

```bash
./install.sh
```

Скрипт собирает Release-версию и устанавливает `filehopz` в
`/usr/local/bin/filehopz`. Если для записи в `/usr/local/bin` нужны права
администратора, скрипт использует `sudo`.

Для удаления установленного бинарника:

```bash
./uninstall.sh
```

## Использование

```bash
Sender:   filehopz send <address> <port> <file/directory>
Receiver: filehopz recv <port> <output_directory>
```

Подробная справка доступна через `filehopz --help`.

После запуска receiver покажет локальные IPv4 адреса с портом в формате
`address:port`, которые можно передать в sender. Перед приёмом receiver выведет
размер передаваемых данных и попросит подтверждение: `y` — принять, `n` —
отказаться (для автоматического принятия можно выставить флаг --auto-accept).
