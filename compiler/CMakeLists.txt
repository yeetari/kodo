add_executable(kodoc
    Analysis.cc
    AstLowering.cc
    CharStream.cc
    Diagnostic.cc
    HirLowering.cc
    Lexer.cc
    main.cc
    Parser.cc
    Token.cc)
target_compile_features(kodoc PRIVATE cxx_std_20)
target_include_directories(kodoc PRIVATE .)
target_link_libraries(kodoc PRIVATE coel fmt::fmt)
