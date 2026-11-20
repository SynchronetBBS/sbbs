state.check(js.options === state.options, "options inside throwing script");
throw new Error("expected scoped exception");
