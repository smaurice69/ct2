# ct2

This project contains a minimal C++ chess engine skeleton. It provides:

- A basic board representation using bitboards
- Magic bitboard attack tables for sliding pieces
- A simple UCI interface
- Unit tests using GoogleTest

## Building

```
cmake -S . -B build
cmake --build build
```

## Running

To start the engine:

```
./build/ct2
```

## Testing

```
cmake --build build --target ct2_tests
./build/ct2_tests
```
