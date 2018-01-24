#include <stack>
#include <string>
#include <functional>
#include <exception>
#include <cassert>
#include <iostream>


class SyntaxError : public std::exception {
};

class UnknownOperator : public std::exception {
};

class OperatorAlreadyDefined : public std::exception {
};

class Lazy {
public:
    Lazy(std::function<int()> f) : func(f) {}
    Lazy(const Lazy &other) : func(other.func) {}
    int operator()() { return func(); }

private:
    std::function<int()> func;
};

class LazyCalculator {
protected:
    typedef int(*LazyFn)(Lazy, Lazy);
private:
    mutable std::stack<Lazy> stack;
    static const int SIGNS = 128;
    std::function<int(Lazy, Lazy)> functions[SIGNS];
    std::function<int()> literals[SIGNS];
    bool is_function_defined[SIGNS];
    bool is_literal_defined[SIGNS];


public:
    LazyCalculator() {
        for (auto &b : is_function_defined) {
            b = false;
        }
        for (auto &b : is_literal_defined) {
            b = false;
        }
        define_literal('0', [](){return 0;});
        define_literal('2', [](){return 2;});
        define_literal('4', [](){return 4;});
        define_function('+', [](Lazy a, Lazy b) {return a() + b();});
        define_function('-', [](Lazy a, Lazy b){return a() - b ();});
        define_function('*', [](Lazy a, Lazy b){return a() * b();});
        define_function('/', [](Lazy a, Lazy b){return a() / b ();});
    }

    void doOperation(char c) const {
        if (is_literal_defined[static_cast<unsigned int>(c)]) {
            Lazy lit = literals[static_cast<unsigned int>(c)];
            stack.push(lit);
        } else if (is_function_defined[static_cast<unsigned int>(c)]) {
            Lazy b = stack.top();
            stack.pop();
            Lazy a = stack.top();
            stack.pop();
            stack.push(Lazy([a, b, this, c](){return this->functions
            [static_cast<unsigned int>(c)](a, b);}));
        } else {
            throw UnknownOperator();
        }
    }


    Lazy parse(const std::string& s) const {
        for_each(s.begin(), s.end(), std::bind(&LazyCalculator::doOperation,
                                               this, std::placeholders::_1));
        if (stack.size() != 1) {
            throw SyntaxError();
        }
        Lazy a = stack.top();
        stack.pop();
        return a;
    }

    int calculate(const std::string& s) const {
        return parse(s)();
    }

    void define(char c, std::function<int(Lazy, Lazy)> fn) {
        if (is_function_defined[static_cast<unsigned int>(c)]
            || is_literal_defined[static_cast<unsigned int>(c)]) {
            throw OperatorAlreadyDefined();
        }
        functions[static_cast<unsigned int>(c)] = std::move(fn);
        is_function_defined[static_cast<unsigned int>(c)] = true;
    }

    void define_function(char c, LazyFn f) {
        define(c, f);
    }

    void define_literal(char c, std::function<int()> fn) {
        if (is_literal_defined[static_cast<unsigned int>(c)]) {
            throw OperatorAlreadyDefined();
        }
        literals[static_cast<unsigned int>(c)] = std::move(fn);
        is_literal_defined[static_cast<unsigned int>(c)] = true;
    }
};

std::function<void(void)> operator*(int n, const std::function<void(void)> &fn) {
    return [=]() {
        for (int i = 0; i < n; i++)
            fn();
    };
}

int manytimes(Lazy n, Lazy fn) {
    (n() * fn)();  // Did you notice the type cast?
    return 0;
}

int main() {
    LazyCalculator calculator;

    // The only literals...
    assert(calculator.calculate("0") == 0);
    assert(calculator.calculate("2") == 2);
    assert(calculator.calculate("4") == 4);

    // Built-in operators.
    assert(calculator.calculate("42+") == 6);
    assert(calculator.calculate("24-") == -2);
    assert(calculator.calculate("42*") == 8);
    assert(calculator.calculate("42/") == 2);

    assert(calculator.calculate("42-2-") == 0);
    assert(calculator.calculate("242--") == 0);
    assert(calculator.calculate("22+2-2*2/0-") == 2);

    // The fun.
    calculator.define('!', [](Lazy a, Lazy b) { return a()*10 + b(); });
    assert(calculator.calculate("42!") == 42);

    std::string buffer;
    calculator.define(',', [](Lazy a, Lazy b) { a(); return b(); });
    calculator.define('P', [&buffer](Lazy, Lazy) { buffer += "pomidor"; return 0; });
    assert(calculator.calculate("42P42P42P42P42P42P42P42P42P42P42P42P42P42P42P4"
                                        "2P,,,,42P42P42P42P42P,,,42P,42P,42P42P,,,,42P,"
                                        ",,42P,42P,42P,,42P,,,42P,42P42P42P42P42P42P42P"
                                        "42P,,,42P,42P,42P,,,,,,,,,,,,") == 0);
    assert(buffer.length() == 42 * std::string("pomidor").length());

    std::string buffer2 = std::move(buffer);
    buffer.clear();
    calculator.define('$', manytimes);
    assert(calculator.calculate("42!42P$") == 0);
    // Notice, how std::move worked.
    assert(buffer.length() == 42 * std::string("pomidor").length());

    calculator.define('?', [](Lazy a, Lazy b) { return a() ? b() : 0; });
    assert(calculator.calculate("042P?") == 0);
    assert(buffer == buffer2);

    assert(calculator.calculate("042!42P$?") == 0);
    assert(buffer == buffer2);

    calculator.define('1', [](Lazy, Lazy) { return 1; });
    assert(calculator.calculate("021") == 1);

    for (auto bad: {"", "42", "4+", "424+"}) {
        try {
            calculator.calculate(bad);
            assert(false);
        }
        catch (SyntaxError) {
        }
    }

    try {
        calculator.define('!', [](Lazy a, Lazy b) { return a()*10 + b(); });
        assert(false);
    }
    catch (OperatorAlreadyDefined) {
    }

    try {
        calculator.define('0', [](Lazy, Lazy) { return 0; });
        assert(false);
    }
    catch (OperatorAlreadyDefined) {
    }

    try {
        calculator.calculate("02&");
        assert(false);
    }
    catch (UnknownOperator) {
    }

    return 0;
}