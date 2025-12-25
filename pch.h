// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

// 测试项目的 pch.h 最上方添加
#ifdef _DEBUG
#undef ASSERT
#undef VERIFY
#endif

// 在所有 #include "gtest/gtest.h" 之前加入
#define GTEST_HAS_TR1_TUPLE 0
#define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING 1

// pch.h 中修改为：

#ifdef _DEBUG
#undef ASSERT  // 取消 MFC 的 ASSERT 宏
#endif

// 然后在包含 gtest 之前不要重新定义


#ifndef PCH_H
#define PCH_H

// 添加要在此处预编译的标头
#include "framework.h"

#endif //PCH_H
