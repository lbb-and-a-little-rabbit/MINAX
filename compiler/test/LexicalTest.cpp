#include "Lexical.h"

int main() {
    std::vector<std::string> testSets;
    // ===== 简单用例 =====
    testSets.push_back("1 + 2 * 3");
    testSets.push_back("1 + 2 * 3 / 4");
    testSets.push_back("1 + 2 * 3 / 4 - 5");
    testSets.push_back("1 + 2 * 3 / 4 - 5 * 6");
    testSets.push_back("x + y - z * w / t");
    testSets.push_back("x + y - z * w / t + sum ( i , 1 , 3 , i * x ) * prod ( i , 1 , 3 , i + y )");

    // ===== 正常用例：数字 =====
    testSets.push_back("0");
    testSets.push_back("123 0.5 .5 5. 1e3 1e-3 2.5E+4"); // 仅测试分词
    testSets.push_back("- 3 + + 2");                       // 正负号单独分词
    testSets.push_back("x * - 2.5");

    // ===== 正常用例：标识符 =====
    testSets.push_back("_x + x1 * alpha_2");
    testSets.push_back("sumValue + product_value");

    // ===== 正常用例：所有比较运算符 =====
    testSets.push_back("x = y");
    testSets.push_back("x != y");
    testSets.push_back("x > y");
    testSets.push_back("x < y");
    testSets.push_back("x >= y");
    testSets.push_back("x <= y");

    // ===== 正常用例：括号、求和、连乘 =====
    testSets.push_back("( x + y ) * ( z - 2 )");
    testSets.push_back("( ( x + 1 ) * y ) / 2");
    testSets.push_back("sum ( i , 1 , 10 , i * i )");
    testSets.push_back("prod ( j , 1 , 4 , x + j )");
    testSets.push_back(
        "sum ( i , 1 , 3 , prod ( j , 1 , 2 , x + i * j ) )"
    );

    // ===== 正常用例：空输入及不同空白 =====
    testSets.push_back("");                    // 只产生 End
    testSets.push_back("   \t\n\r ");           // 只产生 End
    testSets.push_back("   x + 2   ");
    testSets.push_back("x\t+\ty");
    testSets.push_back("x\n+\n2");
    testSets.push_back("x\r\n+\t2");
    testSets.push_back("x\f+\v2");

    // ===== 词法错误：非法字符或不支持的运算符 =====
    testSets.push_back("x @ y");
    testSets.push_back("x % 2");
    testSets.push_back("x ^ 2");
    testSets.push_back("x ! y");
    testSets.push_back("x == y");
    testSets.push_back("x && y");
    testSets.push_back("x ;");

    // ===== 词法错误：数字格式不合法 =====
    testSets.push_back(".");
    testSets.push_back("1.2.3");
    testSets.push_back("1e");
    testSets.push_back("1e+");
    testSets.push_back("1e--3");
    testSets.push_back("12abc");
    testSets.push_back("0x10");                 // 当前不支持十六进制
    testSets.push_back("1e9999");               // 超出 double 范围

    // ===== 词法错误：标识符不合法 =====
    testSets.push_back("abc@");
    testSets.push_back("name$");
    testSets.push_back("2variable");

    // ===== 词法错误：没有按约定用空白分隔 =====
    testSets.push_back("1+2");
    testSets.push_back("-3");
    testSets.push_back("+3");
    testSets.push_back("(x + y)");
    testSets.push_back("x >=3");
    testSets.push_back("sum(i,1,3,i)");
    testSets.push_back("sum ( i, 1 , 3 , i )");

    // ===== 分词成功，但应由语法分析器报错 =====
    testSets.push_back("1 +");                  // 缺少右操作数
    testSets.push_back("* 2");                  // 缺少左操作数
    testSets.push_back("1 + * 2");              // 运算符位置错误
    testSets.push_back("( x + y");             // 缺少右括号
    testSets.push_back("x + y )");              // 多余右括号
    testSets.push_back("( )");                 // 空表达式
    testSets.push_back("x y");                 // 缺少运算符
    testSets.push_back("x > = y");             // 两个合法 Token，组合不合法
    testSets.push_back("sum ( i , 1 , 3 , )");  // 缺少求和表达式

    // ===== 分词成功，问题留给后续检查或求值 =====
    testSets.push_back("1 / 0");                // 求值时检查除零
    testSets.push_back("unknownFunc ( x )");    // 检查函数是否存在

    for (std::size_t i = 0; i < testSets.size(); ++i) {
        std::cout << "\nTest " << i + 1
                << ": \"" << testSets[i] << "\"\n";

        try {
            Lexical lexical(testSets[i]);
            const auto tokens = lexical.tokenize();

            for (const auto& token : tokens) {
                std::cout
                    << "[" << token.position << "] "
                    << (token.kind == TokenKind::End
                            ? "<END>"
                            : token.text)
                    << '\n';
            }
        } 
        catch (const std::exception& e) {
            std::cout << e.what() << '\n';
        }
    }
}