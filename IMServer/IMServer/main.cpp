// IMServer 服务器主程序
// 架构：Kernel（收发数据 + 组织要发送的数据）→ 中介类 TcpServerMed → 网络类 TcpServer → WinSock API
#include <iostream>
#include <Windows.h>
#include "Kernel.h"

using namespace std;

int main() {
	// 控制台默认使用系统代码页（中文系统是 GBK），而源码是 UTF-8 编码，
	// 把控制台输出代码页设为 UTF-8，防止 cout 打印汉字乱码
	SetConsoleOutputCP(CP_UTF8);

	Kernel kernel;

	kernel.startServer();

	// 保持服务器一直运行
	while (true) {
		cout << "server is running" << endl;
		Sleep(50000);
	}

	return 0;
}
