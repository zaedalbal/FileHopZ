# FileHopZ

FileHopZ - это легковесный инструмент для передачи файлов по сети, ориентированный на простоту использования и скорость передачи

## Текущее состояние проекта

На данный момент проект работает только внутри локальной сети

⚠️ На текущем этапе разработки проект не рекомендуется для реального использования ⚠️

## Требования

* Компилятор с поддержкой **C++23**
* **CMake 3.25+**
* **Git**

### Установка зависимостей

**Для Ubuntu/Debian:**

```bash
sudo apt install git cmake g++ libboost-all-dev
```

**Для Fedora:**

```bash
sudo dnf install git cmake gcc-c++ boost-devel
```

**Для Arch Linux / Manjaro:**

```bash
sudo pacman -S git cmake gcc boost
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

## Использование

```bash
Sender:   ./filehopz send <address> <port> <file/directory>
Receiver: ./filehopz recv <port> <output_directory>
```