# vcpkg.json 完整讲解（你这份IM项目配置）
```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "im-project",
  "version": "0.1.0",
  "dependencies": [
    "libmysql"
  ]
}
```
这是 **vcpkg Manifest（清单模式）** 的配置文件，放在**项目根目录**，专门用来声明项目依赖库，就是你截图方案A的核心。

## 逐字段解释
### 1. `"$schema"`
```json
"$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json"
```
- 作用：JSON的**语法校验文件**，VSCode安装vcpkg插件后，会自动读取这个schema，**给你提示、补全、标红语法错误**。
- 可以理解为：告诉编辑器「这个文件是vcpkg的配置，按vcpkg规则校验」。
> 删掉也能跑，但是失去IDE智能提示，建议保留。

### 2. `"name": "im-project"`
- 项目名，**全小写、短横线分隔**，vcpkg规范。
- 只是项目标识，不影响编译；一般写你的项目名字，你的IM项目就叫`im-project`。

### 3. `"version": "0.1.0"`
项目版本号，语义化版本：`主.次.补丁`。
- 0.1.0：早期开发版本，适合你的IM聊天项目。
- 只是标记，不会自动打包发布，仅用于vcpkg清单管理。

### 4. `"dependencies": [ "libmysql" ]` ✅ 核心
`dependencies` = 项目依赖库列表，**数组，可以放多个库**。
```json
"dependencies": [
  "libmysql",
  "protobuf",
  "fmt",
  "qtbase"
]
```
这里写 `"libmysql"`：告诉vcpkg，本项目需要安装`libmysql`库（MySQL客户端库，用来C++代码连数据库）。

> 重点区分：
> `libmysql`：**MySQL客户端库（用来在C++代码操作MySQL）**
> ≠ MySQL Server数据库服务（数据库服务器，单独装）
> 这就是方案A最大优势：**本机不需要安装MySQL服务，vcpkg只下载客户端库**。

---

# 清单模式（Manifest Mode）是什么？
vcpkg两种使用模式：
1. **经典模式（旧）**：全局`vcpkg install libmysql`，库装在vcpkg根目录，**多个项目共用一套库**，换机器容易环境不一致。
2. **清单模式（manifest，你现在用的，推荐）**
    - 项目根目录放`vcpkg.json`，提交进Git仓库
    - 每一个项目拥有**独立的依赖**
    - 别人clone你的IM项目，只需要执行：
    ```bash
    vcpkg install
    ```
    vcpkg自动读取`vcpkg.json`，下载、编译你写的全部依赖（libmysql），放到项目本地`vcpkg_installed`文件夹。
    ✅ 跨机器可复现，解决你之前「绝对路径换机器失效」的痛点。

## 两种写法声明依赖
### 简写（你现在用的，默认动态库）
```json
"dependencies": [ "libmysql" ]
```

### 完整写法（指定静态库、特性）
```json
"dependencies": [
  {
    "name": "libmysql",
    "features": ["static"]
  }
]
```
- `"features":["static"]`：编译**静态版本libmysql**，打包程序时不需要额外带dll；
- 不写features，默认编译动态库（dll）。

> 你的IM项目如果最后要打包exe给别人，优先考虑static静态版本。

---

# 和CMake联动（配套，必须一起写）
光写`vcpkg.json`不够，CMakeLists.txt 要去找这个库：
```cmake
# 查找vcpkg安装的libmysql包
find_package(unofficial-libmysql CONFIG REQUIRED)
# 将库链接到你的程序目标
target_link_libraries(IMServer PRIVATE unofficial::libmysql::libmysql)
```
> 注意包名：`unofficial-libmysql`，不是`libmysql`，这是vcpkg里这个端口的命名。

然后cmake构建时，**必须带上vcpkg工具链文件**：
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
```
`CMAKE_TOOLCHAIN_FILE` 是桥梁，CMake通过这个文件知道去哪里找vcpkg安装的库。

## 源码层面改动（截图里的要求）
原来代码里写了硬编码：
```cpp
#pragma comment(lib, "libmysql.lib")
```
❌ 删掉！
`#pragma comment(lib)`是MSVC专属的编译指令，写死库名，移植性差。现在由CMake+vcpkg统一管理链接。

---

# 常用配套命令
```bash
# 自动添加依赖（不用手写json，推荐）
vcpkg add port libmysql

# 读取vcpkg.json，安装所有依赖
vcpkg install

# 查看已安装依赖
vcpkg list
```

# 可选进阶：版本锁定（多人协作强烈推荐）
在vcpkg.json增加`builtin-baseline`，锁定vcpkg仓库版本，防止不同时间拉取到**不同版本libmysql**，避免“在我电脑能跑，你电脑不行”。
```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "im-project",
  "version": "0.1.0",
  "builtin-baseline": "a1b2c3d4e5f6...",
  "dependencies": [
    "libmysql"
  ]
}
```
baseline哈希值用 `vcpkg x-update-baseline` 自动生成。

---

# 整体工作流程总结（IM项目）
1. 项目根目录：`vcpkg.json` 声明依赖 libmysql
2. 执行`vcpkg install` → vcpkg下载编译libmysql
3. CMakeLists.txt 写 find_package + target_link_libraries
4. cmake构建时传入vcpkg工具链文件
5. C++代码直接`#include <mysql/mysql.h>`，直接使用mysql客户端API

## 常见坑提醒
1. 平台：默认x64-windows；如果编译32位，需要指定triplet
2. 动态库版本：运行exe时，需要把libmysql.dll复制到exe同目录；静态库不需要dll
3. 不要混用经典模式 + 清单模式，容易冲突
