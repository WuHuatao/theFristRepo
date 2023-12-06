#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <vector>
#include <queue>
#include "test.h"

void test1();

using namespace std;

// CPU初始化：
int registers[32]; // 32位寄存器，每位寄存器有4byte，32bit的空间
int memory[2048] = {}; // assuming a word is 4 bytes and memory is 2K bytes
std::deque<int> PCQueue; // 将PC模拟成一个队列，存储的是下n条instruction在内存中的地址。使用双端队列的原因是data hazard需要重新塞入

// 程序参数设置：程序参数设置
int ruunning_time = 30; // 需要设置整个程序的执行时间，时间长度需要cover所有的线程的执行时间。这里默认是30s，理论可以执行26条指令
int thread_number = 5; // 这里可以自己定义设置线程的数量，想要有几个线程，就在初始化PC队列的时候push几个，最大为5
int time_penalty = 0; // 设置遇到data hazard其他线程的时间延时
int hazard_id = 0; // 记录出现branch hazard的线程的id
int data_hazard_id = 0; // 记录出现data hazard的线程的id
int mem_hazard_id = 0; // 记录出现mem hazard的线程的id
std::queue<int> reg; // 存放当前正在使用的reg的id
std::queue<int> mem; // 存放当前正在使用的mem的id

// 工具函数：

// 32位字符串二进制转int类型十进制(测试完成)
int binaryToDecimal(const std::string &binaryString) {
    int decimalNumber = 0;
    int binaryBase = 1; // 用于表示二进制位权重的基数

    // 从字符串的最后一位开始，逐位处理二进制位
    for (int i = binaryString.length() - 1; i >= 0; i--) {
        if (binaryString[i] == '1') {
            decimalNumber += binaryBase;
        }
        binaryBase *= 2; // 每次迭代，基数乘以2，以处理下一位
    }

    return decimalNumber;
}

// 十进制转32位二进制(测试完成)
std::string decimalToBinary32(int decimal) {
    std::bitset<32> binary(decimal);
    return binary.to_string();
}

// 判断元素是否存在于队列中
bool findElementInQueue(const std::queue<int>& myQueue, int element) {
    std::queue<int> copyQueue = myQueue;

    while (!copyQueue.empty()) {
        if (copyQueue.front() == element) {
            return true; // 找到元素
        }
        copyQueue.pop();
    }

    return false; // 未找到元素
}

// 程序函数部分：

class Instruction {

public: // Methods for each pipeline stage

    int IF_Stage(int threadId);
    int* ID_Stage(int threadId, int inst, int PC_inst);
    int* EX_Stage(int threadId, int* arr, int PC_inst);
    int* MEM_Stage(int threadId, int* arr);
    int WB_Stage(int PC_inst, int* arr);

    // Constructor
    Instruction(){
        // Initialize registers and memory
    }
};

// Fetch instruction from memory int类型队列(测试完成)
int Instruction::IF_Stage(int inst){
    cout << "queue size is : " << PCQueue.size() << " --- inst is : " << inst << " ***" << endl;

    if(memory[inst] == 0){ // 下一条instruction为空，则程序结束，返回异常值1
        return 1;
    }

    PCQueue.pop_front(); // 当前instruction执行完毕，弹出队列。需要放在IF_Stage，然后需要放在return 1后面，防止其他无关进程修改
    return memory[inst]; // 读取这条指令的具体内容，并返回
}

// Decode instruction and read registers, return an array with parameters inside
int* Instruction::ID_Stage(int threadId, int inst, int PC_inst) {

    // 判断上一条线程是否出现hazard
    if (PC_inst == hazard_id + 1){ // 如果这一条指令是出现branch hazard的下一条指令
        int* error = new int[1];
        error[0] = 100;
        return error; // 出现异常
    } else if (PC_inst == data_hazard_id + 1){ // 如果这一条指令是出现data hazard的下一条指令
        int* error = new int[1];
        error[0] = 101;
        return error; // 出现异常
    } else if (PC_inst == mem_hazard_id + 2){ // 如果这一条指令是出现mem hazard的下两条指令，判断其第前两条线程是否出现hazard
        int* error = new int[1];
        error[0] = 102;
        return error; // 出现异常
    }

    // 将int类型的instruction转换为String类型，进行decode
    string instString = decimalToBinary32(inst);

    // 创建int类型数组进行返回，装载解析完成的参数
    int* arr = new int[4];

    string Opcode = instString.substr(25, 7); // 首先parse opcode部分，看是什么类型的instruction

    if (Opcode == "0110011") { // 如果是R-type
        // decode instruction
        string funct3 = instString.substr(17, 3); // 根据 funct3 判断具体方法
        string funct7 = instString.substr(0, 7); // 根据 funct7 判断具体方法

        string rs1 = instString.substr(12,5); // 得到 frist souce register number
        string rs2 = instString.substr(7,5); // 得到 second souce register number
        string rd = instString.substr(20,5); // 得到 destination register number

        // 转回int类型，数字是多少，对应的register编号就是多少
        int rs1_reg = binaryToDecimal(rs1);
        int rs2_reg = binaryToDecimal(rs2);
        int rd_reg = binaryToDecimal(rd);

        // 判断其他线程是否出现data hazard or structural hazard
        if (findElementInQueue(reg, rs1_reg) && findElementInQueue(reg,rs2_reg) && findElementInQueue(reg,rd_reg) == false) {
            // 操作的寄存器reg不在已使用的寄存器队列reg中

            // 将要使用的reg编号入队，在队中的reg其他线程无法使用
            reg.push(rs1_reg);
            reg.push(rs2_reg);
            reg.push(rd_reg);
        } else {
            // 操作的寄存器reg有一个或多个在已使用的寄存器队列reg中

            time_penalty = time_penalty + 4; // 如果出现寄存器冲突，则延时4s
            data_hazard_id = PC_inst; // 出现data hazard的指令id
        }

        // 取出对应souce register中存储的值
        // rd register不需要提取值，因为只会向里面写入，不会读取
        int rs1_val = registers[rs1_reg];
        int rs2_val = registers[rs2_reg];

        if(funct3 == "000" && funct7 == "0000000"){ // 判断是否是add
            int operation = 0; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，两个计算值，目标寄存器编码
        }else if(funct3 == "000" && funct7 == "0100000"){ // 判断是否是sub
            int operation = 1; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，两个计算值，目标寄存器编码
        }else if(funct3 == "000" && funct7 == "0000001"){ // 判断是否是mul
            int operation = 2; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，两个计算值，目标寄存器编码
        }else if(funct3 == "001" && funct7 == "0000000"){ // 判断是否是sll
            int operation = 3; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，两个计算值，目标寄存器编码
        }else if(funct3 == "101" && funct7 == "0000000"){ // 判断是否是srl
            int operation = 4; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，两个计算值，目标寄存器编码
        }else if(funct3 == "110" && funct7 == "0000000"){ // 判断是否是or
            int operation = 5; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，两个计算值，目标寄存器编码
        }else if(funct3 == "111" && funct7 == "0000000"){ // 判断是否是and
            int operation = 6; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，两个计算值，目标寄存器编码
        }

    }else if(Opcode == "0000011" || Opcode == "0010011"){ // 如果是I-type
        string funct3 = instString.substr(17, 3); // 根据 funct3 判断具体方法

        string rs1 = instString.substr(12,5);
        string rd = instString.substr(20,5);
        string immed = instString.substr(0,12); // 获取immed[11:0]

        // 转回int类型，数字是多少，对应的register编号就是多少
        int rs1_reg = binaryToDecimal(rs1);
        int rd_reg = binaryToDecimal(rd);
        int imm = binaryToDecimal(immed);

        // 判断其他线程是否出现data hazard or structural hazard
        if (findElementInQueue(reg, rs1_reg) && findElementInQueue(reg,rd_reg) == false) {
            // 操作的寄存器reg不在已使用的寄存器队列reg中

            // 将要使用的reg编号入队，在队中的reg其他线程无法使用
            reg.push(rs1_reg);
            reg.push(rd_reg);
        } else {
            // 操作的寄存器reg有一个或多个在已使用的寄存器队列reg中

            time_penalty = time_penalty + 4; // 如果出现寄存器冲突，则延时4s
            data_hazard_id = PC_inst; // 出现data hazard的指令id
        }

        // 取出对应souce register中存储的值
        int rs1_val = registers[rs1_reg];

        if(funct3 == "010"){ // 判断是否是lw
            int operation = 7; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = imm;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，一个计算值，immediate，目标寄存器编码
        }else if(funct3 == "000"){ // 判断是否是addi
            int operation = 8; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = imm;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，一个计算值，immediate，目标寄存器编码
        }else if(funct3 == "110"){ // 判断是否是ori
            int operation = 9; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = imm;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，一个计算值，immediate，目标寄存器编码
        }else if(funct3 == "111"){ // 判断是否是andi
            int operation = 10; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = imm;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，一个计算值，immediate，目标寄存器编码
        }else if(funct3 == "010"){ // 判断是否是slti
            int operation = 11; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = imm;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，一个计算值，immediate，目标寄存器编码
        }else if(funct3 == "011"){ // 判断是否是sltiu
            int operation = 12; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = imm;
            arr[3] = rd_reg;

            return arr; // 函数结束，返回：操作类型，一个计算值，immediate，目标寄存器编码
        }
    }else if(Opcode == "0100011" || Opcode == "1100111"){  // 如果是S-type & SB-type
        string funct3 = instString.substr(17, 3); // 根据 funct3 判断具体方法

        string immed2 = instString.substr(0, 7); // 获取immed[11:5]
        string rs1 = instString.substr(12,5); // 得到 frist souce register number
        string rs2 = instString.substr(7,5); // 得到 second souce register number
        string immed1 = instString.substr(20,5); // 获取immed[4:0]

        // 转回int类型，数字是多少，对应的register编号就是多少
        int rs1_reg = binaryToDecimal(rs1);
        int rs2_reg = binaryToDecimal(rs2);

        // 判断其他线程是否出现data hazard or structural hazard
        if (findElementInQueue(reg, rs1_reg) && findElementInQueue(reg,rs2_reg) == false) {
            // 操作的寄存器reg不在已使用的寄存器队列reg中

            // 将要使用的reg编号入队，在队中的reg其他线程无法使用
            reg.push(rs1_reg);
            reg.push(rs2_reg);
        } else {
            // 操作的寄存器reg有一个或多个在已使用的寄存器队列reg中

            time_penalty = time_penalty + 4; // 如果出现寄存器冲突，则延时4s
            data_hazard_id = PC_inst; // 出现data hazard的指令id
        }

        // 将要使用的reg编号入队，在队中的reg其他线程无法使用
        reg.push(rs1_reg);
        reg.push(rs2_reg);

        // 计算immediate的值：
        string immed = immed1 + immed2;
        int imm = binaryToDecimal(immed);

        // 取出对应souce register中存储的值
        int rs1_val = registers[rs1_reg];
        int rs2_val = registers[rs2_reg];

        if(funct3 == "010"){ // 判断是否是sw
            int operation = 13; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = imm;

            return arr; // 函数结束，返回：操作类型，两个计算值，immediate
        }else if(funct3 == "000"){ // 判断是否是beq
            int operation = 14; // 操作类型

            // 将参数封装在一个int数组中进行函数间的传递
            arr[0] = operation;
            arr[1] = rs1_val;
            arr[2] = rs2_val;
            arr[3] = imm;

            time_penalty = time_penalty + 4; // 如果是跳转指令，则延时4s
            hazard_id = PC_inst; // 出现branch hazard的指令id

            return arr; // 函数结束，返回：操作类型，两个计算值，immediate
        }
    }else if(Opcode == "0110111"){  // 如果是U-type，则为lui
        string rd = instString.substr(20,5);
        string immed = instString.substr(0,20); // 获取immed[31:12]

        int rd_reg = binaryToDecimal(rd); // 转回int类型，数字是多少，对应的目标register编号就是多少
        int imm = binaryToDecimal(immed); // 将string类型的immediate转为int类型

        // 判断其他线程是否出现data hazard or structural hazard
        if (findElementInQueue(reg,rd_reg) == false) {
            // 操作的寄存器reg不在已使用的寄存器队列reg中

            // 将要使用的reg编号入队，在队中的reg其他线程无法使用
            reg.push(rd_reg);
        } else {
            // 操作的寄存器reg有一个或多个在已使用的寄存器队列reg中

            time_penalty = time_penalty + 4; // 如果出现寄存器冲突，则延时4s
            data_hazard_id = PC_inst; // 出现data hazard的指令id
        }

        // 将要使用的reg编号入队，在队中的reg其他线程无法使用
        reg.push(rd_reg);

        int operation = 15; // 操作类型

        // 将参数封装在一个int数组中进行函数间的传递
        arr[0] = operation;
        arr[1] = rd_reg;
        arr[2] = imm;

        return arr; // 函数结束，返回：操作类型，目标寄存器，immediate
    }else{
        cout << "Error: there is no matching instruction" << endl;
    }

    return 0;
}

// Execute instruction or calculate address
int* Instruction::EX_Stage(int threadId, int* arr, int PC_inst) {

    // 创建int类型数组进行返回，装载解析完成的参数
    int* param = new int[3];

    // 解析从ID_Stage传递来的int数组arr

    // 首先判断是什么操作
    int operation = arr[0];

    if(operation == 0){ // add
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int rd_reg = arr[3];

        int sum = rs1_val + rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = sum;
        param[2] = rd_reg;

        return param;
    }else if(operation == 1){ // sub
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int rd_reg = arr[3];

        int sub = rs1_val - rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = sub;
        param[2] = rd_reg;

        return param;
    }else if(operation == 2){ // mul
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int rd_reg = arr[3];

        int mul = rs1_val * rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = mul;
        param[2] = rd_reg;

        return param;
    }else if(operation == 3){ // sll
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int rd_reg = arr[3];

        int sll = rs1_val << rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = sll;
        param[2] = rd_reg;

        return param;
    }else if(operation == 4){ // srl
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int rd_reg = arr[3];

        int srl = rs1_val >> rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = srl;
        param[2] = rd_reg;

        return param;
    }else if(operation == 5){ // or
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int rd_reg = arr[3];

        // 执行OR位操作
        int result = rs1_val | rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = rd_reg;

        return param;
    }else if(operation == 6){ // and
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int rd_reg = arr[3];

        // 执行 AND 位操作
        int result = rs1_val & rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = rd_reg;

        return param;
    }else if(operation == 7){ // lw
        int rs1_val = arr[1];
        int imm = arr[2];
        int rd_reg = arr[3];

        int address = rs1_val + imm; // 计算实际内存地址（这个stage只计算地址，下一个stage才进行memory的读写操作）

        // 内存的hazard处理，判断其他线程是否操作同一块内存区域
        if (findElementInQueue(mem, address)){
            // 操作的内存地址mem不在已使用的内存队列mem中

            mem.push(address);
        } else {
            // 操作的内存地址mem在已使用的内存队列mem中

            time_penalty = time_penalty + 3; // 如果出现寄存器冲突，则延时3s
            mem_hazard_id = PC_inst; // 出现mem hazard的指令id

            // 返回一个错误码数组
            int* error = new int[1];
            error[0] = 103;
            return error; // 出现异常
        }

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = address;
        param[2] = rd_reg;

        return param;
    }else if(operation == 8){ // addi
        int rs1_val = arr[1];
        int imm = arr[2];
        int rd_reg = arr[3];

        int sum = rs1_val + imm; // 计算寄存器中存储的值和immediate的sum

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = sum;
        param[2] = rd_reg;

        return param;
    }else if(operation == 9){ // ori
        int rs1_val = arr[1];
        int imm = arr[2];
        int rd_reg = arr[3];

        int result = rs1_val | imm;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = rd_reg;

        return param;
    }else if(operation == 10){ // andi
        int rs1_val = arr[1];
        int imm = arr[2];
        int rd_reg = arr[3];

        int result = rs1_val & imm;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = rd_reg;

        return param;
    }else if(operation == 11){ // slti
        int rs1_val = arr[1];
        int imm = arr[2];
        int rd_reg = arr[3];

        int result = (rs1_val < imm) ? 1 : 0; // 如果 rs1_val < imm，则将 1 存储到 result，否则将 0 存储到 result

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = rd_reg;

        return param;
    }else if(operation == 12){ // sltiu
        int rs1_val = arr[1];
        int imm = arr[2];
        int rd_reg = arr[3];

        unsigned int rs1_unsigned_val = (unsigned int)rs1_val; // 将寄存器中存储的值转换为unsigned类型
        unsigned int imm_unsigned_val = (unsigned int)imm; // 将immediate的值转换为unsigned类型

        int result = (rs1_unsigned_val < imm_unsigned_val) ? 1 : 0;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = rd_reg;

        return param;
    }else if(operation == 13){ // sw
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int imm = arr[3];

        int address = rs1_val + imm; // 计算实际内存地址

        // 内存的hazard处理，判断其他线程是否操作同一块内存区域
        if (findElementInQueue(mem, address)){
            // 操作的内存地址mem不在已使用的内存队列mem中

            mem.push(address);
        } else {
            // 操作的内存地址mem在已使用的内存队列mem中

            time_penalty = time_penalty + 3; // 如果出现寄存器冲突，则延时3s
            mem_hazard_id = PC_inst; // 出现mem hazard的指令id

            // 返回一个错误码数组
            int* error = new int[1];
            error[0] = 104;
            return error; // 出现异常
        }

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = address;
        param[2] = rs2_val;

        return param;
    }else if(operation == 14){ // beq
        int rs1_val = arr[1];
        int rs2_val = arr[2];
        int imm = arr[3];

        // 这一步是跳转指令。如果比较条件成立则跳转到imm指向的PC地址
        int result = rs1_val == rs2_val;

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = imm;

        return param;
    }else if(operation == 15){ // lui
        int rd_reg = arr[1];
        int imm = arr[2];

        // registers[rd_reg] = imm << 12;

        int result = imm << 12; // 将立即数左移 12 位并加载到目标寄存器的高位，将低位部分设置为零

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = result;
        param[2] = rd_reg;

        return param;
    }else{
        cout << "Error: problem occurs in EX_Stage" << endl;
    }

    return 0;
}

// Memory access for load/store instructions
int* Instruction::MEM_Stage(int threadId, int* arr) {

    // 创建int类型数组进行返回，装载解析完成的参数
    int* param = new int[3];

    // 解析从EX_Stage传递来的int数组arr

    // 首先判断是什么操作
    int operation = arr[0];

    // 其实这里只有sw和lw instruction对内存有操作
    if(operation == 7) // lw
    {
        int address = arr[1];
        int rd_reg = arr[2];

        int loadedWord = memory[address]; // 从内存中加载数据，但是要到下个stage才能将加载的数据存储在相应的寄存器中

        // 将参数封装在一个int数组中进行函数间的传递
        param[0] = operation;
        param[1] = loadedWord;
        param[2] = rd_reg;

        return param;
    }else if (operation == 13) // sw
    {
        int address = arr[1];
        int rs2_val = arr[2];

        memory[address] = rs2_val; // 将数据存储到内存中

        return arr; // 实际上该instruction已经完成了，没有啥需要返回的
    }else{
        return arr; // 该instruction不需要进行memory有关的操作，直接返回结束该stage
    }

    return 0;
}

// Write result back to registers
int Instruction::WB_Stage(int PC_inst, int* arr) {

    // 解析从EX_Stage和MEM_Stage传递来的int数组arr

    // 首先判断是什么操作
    int operation = arr[0];

    if (operation == 0){ // add
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 1){ // sub
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 2){ // mul
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 3){ // sll
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 4){ // srl
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 5){ // or
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 6){ // and
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 7){ // lw
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 8){ // addi
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 9){ // ori
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 10){ // andi
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 11){ // slti
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 12){ // sltiu
        unsigned int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将结果写回寄存器

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，R-type是用了三个寄存器，所以弹出三次
        reg.pop();
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 13){ // sw
        // 其实不需要了，sw指令到这之前就已经结束了

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }else if(operation == 14){ // beq
        int result = arr[1];
        int imm = arr[2];

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，S-type是用了两个寄存器，所以弹出两次
        reg.pop();
        reg.pop();

        if (result == 1){ // 判断branch条件是否成立，成立程序则跳转到指定instruction的地址。不成立则不变
            PCQueue.push_back(imm); // 将跳转位置的instruction的地址放入PC队列中
        }else{
            PCQueue.push_front(PC_inst + 1); // 将后一个thread停掉一个cycle的指令重新放回PC队列的头部，因为已经被从PC弹出去了，现在因为判断失败所以需要加回来。PC_inst是当前thread的id，PC_inst + 1就是下一条指令的id
            PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
        }
    }else if(operation == 15){ // lui
        int result = arr[1];
        int rd_reg = arr[2];

        registers[rd_reg] = result; // 将立即数左移 12 位并加载到目标寄存器的高位，将低位部分设置为零

        // 将使用完的reg编号出队，现在这些reg其他线程可以使用，U-type是用了一个寄存器，所以弹出一次
        reg.pop();

        PCQueue.push_back(PCQueue.back() + 1); // 将下一个instruction的地址放入PC队列中
    }

    // 对于出现问题的hazard thread，在结束的时候将hazard设置为0
    hazard_id = 0;
    data_hazard_id = 0;
    mem_hazard_id = 0;

    return 0;
}


// 这个函数一次性执行五个stage的过程(测试完成)
int simulate() {
    int threadId = 0; // 其实没有啥意义，之后没有对这个参数的操作

    // Initialize instruction objects
    Instruction instruction;

    std::this_thread::sleep_for(std::chrono::seconds( time_penalty));

    // stage 1
    int PC_inst = PCQueue.front(); // 读取队列中的出队指令的地址
    int inst = instruction.IF_Stage(PC_inst);
    if(inst == 1){ // 后面没有程序了，程序结束，返回异常值1
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds( 1));

    // stage 2
    int* arr = instruction.ID_Stage(threadId, inst, PC_inst); // 将fetch方法得到的instruction传递到stage 2进行decode。返回一个int数组的指针

    // 是对hazard的下一条thread来执行的：
    if(arr[0] == 100){ // 如果出现错误代码100问题（hazard），这个线程直接走完该走的延时，然后结束
         std::this_thread::sleep_for(std::chrono::seconds( 4));
         return 0;
    }else if (arr[0] == 101){ // 如果出现错误代码101问题（data hazard & structural hazard），这个线程直接走完该走的延时，然后结束
        PCQueue.push_front(PC_inst + 1); // 将后一个thread停掉一个cycle的指令重新放回PC队列的头部，因为已经被从PC弹出去了，现在因为寄存器冲突了所以需要加回来。PC_inst是当前thread的id，PC_inst + 1就是下一条指令的id
        std::this_thread::sleep_for(std::chrono::seconds( 4));
        return 0;
    }else if (arr[0] == 102){ // 如果出现错误代码102问题（mem hazard），这个线程直接走完该走的延时，然后结束
        std::this_thread::sleep_for(std::chrono::seconds( 4));
        return 0;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // stage 3
    int* arr2 = instruction.EX_Stage(threadId, arr, PC_inst);

    if (arr2[0] == 103){ // 如果出现错误代码103问题（mem hazard），这个线程直接走完该走的延时，然后结束
        PCQueue.push_front(PC_inst + 2); // 将后一个thread停掉一个cycle的指令重新放回PC队列的头部，因为已经被从PC弹出去了，现在因为寄存器冲突了所以需要加回来。PC_inst是当前thread的id，PC_inst + 1就是下一条指令的id
        PCQueue.push_front(PC_inst + 1); // 将后一个thread停掉一个cycle的指令重新放回PC队列的头部，因为已经被从PC弹出去了，现在因为寄存器冲突了所以需要加回来。PC_inst是当前thread的id，PC_inst + 1就是下一条指令的id
        std::this_thread::sleep_for(std::chrono::seconds( 3));
        return 0;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // stage 4
    int* arr3 = instruction.MEM_Stage(threadId, arr2);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // stage 5
    instruction.WB_Stage(PC_inst, arr3);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}

// 一个线程中多个5阶段连续执行(测试完成)
void pipeline(int id, int delaySeconds){
    std::this_thread::sleep_for(std::chrono::seconds(delaySeconds)); // 根据delaySeconds延时

    for(int i = 0; PCQueue.size() != 0; i++){
        cout<< "*** Thread " << id << " / cycle: " << i + 1 << " ----- " << flush;
        if(simulate() == 1){
            break;
        }
    }
    cout << "           *** Thread " << id << " END ***" << endl;
}

// 该函数负责5个线程的创建及启动，设置初始的偏离值(开始测试)
void instructionMode(){
    // 创建并启动5个线程，延时时间逐渐增加
    for (int i = 0; i < 5; ++i) {
        std::thread(pipeline, i, i).detach();
    }

    // 等待一段时间以保证所有线程都有机会执行，所有线程结束主线程才能结束
    std::this_thread::sleep_for(std::chrono::seconds(ruunning_time));
}

// 读取输入文件内容，转换为二进制并以int类型存储在内存中(测试完成)
int readFile(){
    string path;
    string line;

    cout << "give the path of file you want to simulate:" << endl;
    std::getline(std::cin, path); // 使用 std::getline 读取用户输入的字符串
    std::ifstream inputFile(path); // 打开该路径的文本文件

    if (inputFile.is_open()) { // 检查文件是否成功打开
        for(int i = 0; std::getline(inputFile, line); i++){ // 逐行读取文件内容
            string subline = line.substr(0,32); // 截取前32位，防止出现最后换行符‘\r’，影响转换结果
            memory[i] = binaryToDecimal(subline); // 将二进制字符串转换为int类型，并存储在相应的memory地址中
        }
    }else{
        std::cerr << "Error: Failed to open the file." << std::endl;
        return 1; // 返回错误代码
    }

    inputFile.close(); // 关闭文件
    return 0;
}

// 初始化PCQueue队列
void PCInitialization(){
    for(int i = 0; i < thread_number; i++){
        PCQueue.push_back(i);
    }
}

// user interface: 用户选择程序执行模式(测试完成)
int modeSelection(){
    char mode;
    int num;

    cout << "do you want to run this program in instruction mode(i) or cycle mode(c)?" << endl;
    cin >> mode;

    if(mode == 'i'){ // 选择instruction mode
        cout << "choose how many thread you want to have in your simulation? , from 1 to 5, number only: " << endl;
        cin >> num; // 用户输入线程的数量

        if(1 < num && 5 >= num){
            cout << "ok, this program running in instruction mode, thread number: " << thread_number << endl;
            thread_number = num;
            PCInitialization(); // 运行函数以初始化PCQueue队列
            instructionMode();
        }else{
            cout << "Error: invalid input detected, this program ended." << endl;
            return 1; // 返回错误代码
        }
    }else if (mode == 'c'){ // 选择cycle mode
        cout << "ok, this program running in cycle mode." << endl;
        thread_number = 1;

        PCInitialization(); // 运行函数以初始化PCQueue队列

        instructionMode(); // 还是执行instructionMode()方法，模式之间的不同只由线程的数量决定
    }else{ // 错误输入，程序结束
        cout << "Error: invalid input detected, this program ended." << endl;
        return 1; // 返回错误代码
    }
    return 0;
}
void test(){
    // 配置程序运行参数：
    ruunning_time = 12; // 设置线程执行时间，默认30s

    // 寄存器初始化：
    registers[10] = 3;
    registers[11] = 4;
    registers[12] = 1;

    int flag = readFile(); // read binary file, then load to the memory1。如果文件打开成功则返回0，失败返回1

    if(flag == 0){ // 判断文件是否打开成功，如果成功则执行后面的函数
        modeSelection(); // mode selection
    }

    // 输出所有寄存器的值：
    // 计算数组的长度
    int length = sizeof(registers) / sizeof(registers[0]);

    // 使用循环输出数组的元素
    for (int i = 0; i < length; ++i) {
        std::cout << "Element " << i << ": " << registers[i] << std::endl;
    }
}


int main(){
    // 开始运行
    test1();

    // 下面是测试代码：
//    cout << " ***** test begin ***** " << endl;
//
//    memory[0] = 5243539;
//    memory[1] = 5243539;
//    memory[2] = 5243539;
//    memory[3] = 5243539;
//    memory[4] = 5243539;
//    memory[5] = 5243539;
//    memory[6] = 5243539;
//    memory[7] = 5243539;
//    memory[8] = 5243539;
//    memory[9] = 5243539;

//    这里可以自己定义设置线程的数量，想要有几个线程，就在初始化PC队列的时候push几个，最大为5
//    PCQueue.push(0);
//    PCQueue.push(1);
//    PCQueue.push(2);
//    PCQueue.push(3);
//    PCQueue.push(4);


//    函数逐stage测试
//    Instruction instruction;
//
//    int b = instruction.IF_Stage(0); // stage 1测试完成
//    int* c = instruction.ID_Stage(0, b); // stage 2测试完成
//    cout << "c0 : " << c[0]  << endl; // 8 为addi操作
//    cout << "c1 : " << c[1]  << endl; // 0 rs1_val
//    cout << "c2 : " << c[2]  << endl; // 5 imm
//    cout << "c3 : " << c[3]  << endl; // 5 reg
//    int* d = instruction.EX_Stage(0, c);// stage 3测试完成
//    cout << "d0 : " << d[0]  << endl; // 8 addi
//    cout << "d1 : " << d[1]  << endl; // 5 sum
//    cout << "d2 : " << d[2]  << endl; // 5 reg
//    int* f = instruction.MEM_Stage(0, d); // stage 4测试完成
//    cout << "f0 : " << f[0]  << endl; // 8
//    cout << "f1 : " << f[1]  << endl; // 5
//    cout << "f2 : " << f[2]  << endl; // 5
//    instruction.WB_Stage(0, f); // stage 5开始测试
//    cout << "寄存器5中的值为： " << registers[5] << endl;

//  simulate函数测试
//    int a = 0;
//    cout << "寄存器5中的值为： " << registers[5] << endl;
//    simulate(&a);
//    cout << "寄存器5中的值为： " << registers[5] << endl;

//  pipeline()函数测试
//    pipeline(0, 0);

//  instructionMode()函数测试
//    instructionMode();

//  读取文件测试
//    for(int i = 0; memory[i];i++){
//        cout << "memory " << i << " -- " << memory[i] << endl;
//    }
    return 0;
}





