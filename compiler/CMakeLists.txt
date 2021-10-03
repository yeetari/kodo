add_executable(kodoc
    CharStream.cc
    IrGen.cc
    Lexer.cc
    main.cc
    Parser.cc
    Token.cc)
target_compile_features(kodoc PRIVATE cxx_std_20)
target_include_directories(kodoc PRIVATE .)
target_link_libraries(kodoc PRIVATE coel fmt::fmt)
