#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "key.h"
#include "usmart.h"
#include "sram.h"
#include "malloc.h"
#include "w25qxx.h"
#include "sdio_sdcard.h"
#include "ff.h"
#include "exfuns.h"
#include "fontupd.h"
#include "text.h"
#include "piclib.h"
#include "string.h"
#include "math.h"
#include "dcmi.h"
#include "ov2640.h"
#include "beep.h"
#include "timer.h"
#include "touch.h"
#include "atk_frec.h"

#include "rtc.h"
#define KEY_WIDTH   40      // 按键宽度
#define KEY_HEIGHT  40      // 按键高度
#define KEY_MARGIN  5       // 按键间距
#define KEY_ROW1_Y  280     // 第一排按键的Y坐标
#define KEY_ROW2_Y  330     // 第二排按键的Y坐标
#define KEY_ROW3_Y  380     // 第三排按键的Y坐标
#define KEY_ROW4_Y  430     // 第四排按键的Y坐标
// 键盘布局
const char *keyboard[4] = {
    "1234567890",    // 第一排
    "qwertyuiop",    // 第二排
    "asdfghjkl",     // 第三排
    "zxcvbnm<--"     // 第四排
};

#define OK_BTN_X 100
#define OK_BTN_Y 480
#define OK_BTN_W 120
#define OK_BTN_H 40
#define OK_KEY '#' 

// 特殊键定义
#define BACKSPACE_KEY '~'  // 回退键特殊标记

#define MAX_NAME_LEN 16    // 姓名最大长度
#define MAX_PHONE_LEN 16   // 电话最大长度
#define MAX_FACES 50      // 最大人脸数量

typedef struct {
    u8 id;                 // 人脸编号
    u16 name[MAX_NAME_LEN]; // 姓名
    u16 phone[MAX_PHONE_LEN]; // 电话
} FaceInfo;

// 人脸信息数据库
FaceInfo faceDatabase[MAX_FACES];
u8 faceCount = 0;          // 当前存储的人脸数量

void FaceDB_Init(void)
{
    faceCount = 0;
    memset(faceDatabase, 0, sizeof(faceDatabase));
}

int reg_time;
u8 ov2640_mode = 0; // 工作模式:0,RGB565模式;1,JPEG模式
u16 *pixdatabuf; // 图像缓存
u8 *namedatabuf;//姓名缓存
u8 *phonedatabuf;//电话缓存

u8 msgbuf[30]; // 消息缓存区

// 处理JPEG数据
// 当采集完一帧JPEG数据后,调用此函数,切换JPEG BUF.开始下一帧采集.
// 显示图片（支持缩放）
// x,y: 图片显示起始坐标
// w,h: 原始图片宽度和高度
// scale: 缩放倍数（例如3表示放大3倍）
// data: 图片数据缓存区
void frec_show_picture(u16 x, u16 y, u16 w, u16 h, float scale, u16 *data)
{
    u16 i, j;
    u16 scaled_w = w * scale;  // 缩放后的宽度
    u16 scaled_h = h * scale;  // 缩放后的高度
    
    for (i = 0; i < scaled_h; i++)
    {
        for (j = 0; j < scaled_w; j++)
        { 
            // 计算原始图像对应的像素位置
            u16 src_x = j / scale;
            u16 src_y = i / scale;
            
            // 确保不越界
            if(src_x < w && src_y < h)
            {
                LCD_Fast_DrawPoint(x + j, y + i, data[src_y * w + src_x]);
            }
        }
    }
}
// 读取原始图片数据
// dbuf:数据缓存区
// xoff,yoff:要读取的图像区域起始坐标
// xsize:要读取的图像区域宽度
// width:要读取的宽度(宽高比恒为3:4)
void frec_get_image_data(u16 *dbuf, u16 xoff, u16 yoff, u16 xsize, u16 width)
{
    int w, h;
    u16 height = width * 4 / 3;
    float scale = (float)xsize / width;
    for (h = 0; h < height; h++)
    {
        for (w = 0; w < width; w++)
        {
            dbuf[h * width + w] = LCD_ReadPoint(xoff + w * scale, yoff + h * scale);
        }
    }
}

void jpeg_data_process(void)
{
    if (ov2640_mode) // 只有在JPEG格式下,才需要做处理.
    {
    }
}

// 全局变量
u8 name_selected = 1; // 默认选中姓名输入框

// 显示输入框
void show_input_boxes(void)
{
    // 清除原有显示区域
    LCD_Fill(50, 100, 50 + 200, 100 + 60, WHITE);
    LCD_Fill(50, 150, 50 + 200, 150 + 60, WHITE);
    
    // 绘制姓名输入框
    if(name_selected) {
        LCD_DrawRectangle(45, 95, 45 + 210, 95 + 70); // 外框
        LCD_DrawRectangle(47, 97, 47 + 206, 97 + 66); // 内框
    }
    LCD_ShowString(50, 100, 200, 60, 24, "NAME:");
    LCD_ShowString(160, 100, 200, 24, 24, (u8 *)namedatabuf);
    
    // 绘制电话输入框
    if(!name_selected) {
        LCD_DrawRectangle(45, 145, 45 + 210, 145 + 70); // 外框
        LCD_DrawRectangle(47, 147, 47 + 206, 147 + 66); // 内框
    }
    LCD_ShowString(50, 150, 200, 60, 24, "TELEPHONE:");
    LCD_ShowString(160, 150, 200, 24, 24, (u8 *)phonedatabuf);
}

// 全局变量
u8 name_input_pos = 0;  // 当前输入位置
u8 phone_input_pos = 0; // 当前输入位置
u8 current_input = 0;   // 0-姓名输入, 1-电话输入

// 检测键盘输入
// 返回: 按下的键值, 0表示没有按键按下
u8 check_keyboard_input(void)
{
    static u32 last_press_time = 0;
    static u8 last_key = 0;
    static u8 key_repeat = 0;
    u32 current_time = SysTick->VAL;
    u8 i, j;
    u16 x, y;
    u16 start_x, row_y, key_x;
    char key_str[2] = {'\0'};
    
    if (tp_dev.sta & TP_PRES_DOWN) {
        x = tp_dev.x[0];
        y = tp_dev.y[0];
        
        // 检查是否在键盘区域
        if (y >= KEY_ROW1_Y && y <= KEY_ROW4_Y + KEY_HEIGHT) {
            for (i = 0; i < 4; i++) {
                const char *row = keyboard[i];
                u8 row_len = strlen(row);
                start_x = (lcddev.width - (row_len * (KEY_WIDTH + KEY_MARGIN) - KEY_MARGIN)) / 2;
                row_y = KEY_ROW1_Y + i * (KEY_HEIGHT + KEY_MARGIN);
                
                if (y >= row_y && y <= row_y + KEY_HEIGHT) {
                    for (j = 0; j < row_len; j++) {
                        key_x = start_x + j * (KEY_WIDTH + KEY_MARGIN);
                        
                        if (x >= key_x && x <= key_x + KEY_WIDTH) {
                            u8 current_key = (row[j] == '<') ? BACKSPACE_KEY : row[j];
                            
                            // 消抖处理
                            if(current_key != last_key) {
                                last_key = current_key;
                                last_press_time = current_time;
                                key_repeat = 0;
                            } else {
                                // 连续按键处理
                                if(current_time - last_press_time > (key_repeat ? 100000 : 300000)) {
                                    last_press_time = current_time;
                                    key_repeat = 1;
                                } else {
                                    return 0;
                                }
                            }
                            
                            // 高亮显示按下的键
                            LCD_Fill(key_x + 1, row_y + 1, key_x + KEY_WIDTH - 1, row_y + KEY_HEIGHT - 1, BLUE);
                            
                            // 设置按键文字
                            key_str[0] = row[j];
                            if (key_str[0] == '<') key_str[0] = 0x7F;
                            key_str[1] = '\0';
                            
                            POINT_COLOR = BLACK;
                            LCD_ShowString(key_x + (KEY_WIDTH - 8) / 2, row_y + (KEY_HEIGHT - 16) / 2, 16, 16, 16, (u8 *)key_str);
                            
                            delay_ms(50); // 缩短高亮时间
                            
                            // 恢复按键背景
                            LCD_Fill(key_x + 1, row_y + 1, key_x + KEY_WIDTH - 1, row_y + KEY_HEIGHT - 1, GRAY);
                            LCD_ShowString(key_x + (KEY_WIDTH - 8) / 2, row_y + (KEY_HEIGHT - 16) / 2, 16, 16, 16, (u8 *)key_str);
                            
                            return current_key;
                        }
                    }
                }
            }
        }
        
        // 检查是否点击了OK按钮
        if (x >= OK_BTN_X && x <= OK_BTN_X + OK_BTN_W && 
            y >= OK_BTN_Y && y <= OK_BTN_Y + OK_BTN_H) {
            // 按钮按下效果
            LCD_Fill(OK_BTN_X + 1, OK_BTN_Y + 1, OK_BTN_X + OK_BTN_W - 1, OK_BTN_Y + OK_BTN_H - 1, BLUE);
            LCD_ShowString(OK_BTN_X + (OK_BTN_W - 48)/2, OK_BTN_Y + (OK_BTN_H - 24)/2, 48, 24, 24, "OK");
            delay_ms(100);
            LCD_Fill(OK_BTN_X + 1, OK_BTN_Y + 1, OK_BTN_X + OK_BTN_W - 1, OK_BTN_Y + OK_BTN_H - 1, GREEN);
            LCD_ShowString(OK_BTN_X + (OK_BTN_W - 48)/2, OK_BTN_Y + (OK_BTN_H - 24)/2, 48, 24, 24, "OK");
            return OK_KEY;
        }
    } else {
        last_key = 0;
    }
    
    return 0;
}

// 处理姓名输入
void handle_name_input(u8 key)
{
    if (key == BACKSPACE_KEY) { // 回退键
        if (name_input_pos > 0) {
            name_input_pos--;
            ((u8 *)namedatabuf)[name_input_pos] = '\0';
        }
    } 
    else if (name_input_pos < MAX_NAME_LEN - 1) { // 普通键
        ((u8 *)namedatabuf)[name_input_pos] = key;
        name_input_pos++;
        ((u8 *)namedatabuf)[name_input_pos] = '\0';
    }
    
    // 更新显示
    LCD_Fill(160, 100, 160 + 200, 100 + 24, WHITE);
    LCD_ShowString(160, 100, 200, 24, 24, (u8 *)namedatabuf);
}

// 处理电话输入
void handle_phone_input(u8 key)
{
    if (key == BACKSPACE_KEY) { // 回退键
        if (phone_input_pos > 0) {
            phone_input_pos--;
            ((u8 *)phonedatabuf)[phone_input_pos] = '\0';
        }
    } 
    else if (phone_input_pos < MAX_PHONE_LEN - 1) { // 普通键
        ((u8 *)phonedatabuf)[phone_input_pos] = key;
        phone_input_pos++;
        ((u8 *)phonedatabuf)[phone_input_pos] = '\0';
    }
    
    // 更新显示
    LCD_Fill(160, 150, 160 + 200, 150 + 24, WHITE);
    LCD_ShowString(160, 150, 200, 24, 24, (u8 *)phonedatabuf);
}

// 修改键盘UI函数
void keyboard_UI()
{
    u8 i, j;
    u16 x, y;
    u16 start_x, row_y;
    char key_str[2] = {'\0'};
    
    // 绘制四排按键
    for (i = 0; i < 4; i++) {
        const char *row = keyboard[i];
        u8 row_len = strlen(row);
        
        // 计算起始X坐标使键盘居中
        start_x = (lcddev.width - (row_len * (KEY_WIDTH + KEY_MARGIN) - KEY_MARGIN)) / 2;
        
        for (j = 0; j < row_len; j++) {
            x = start_x + j * (KEY_WIDTH + KEY_MARGIN);
            y = KEY_ROW1_Y + i * (KEY_HEIGHT + KEY_MARGIN);
            
            // 绘制按键背景
            LCD_DrawRectangle(x, y, x + KEY_WIDTH, y + KEY_HEIGHT);
            LCD_Fill(x + 1, y + 1, x + KEY_WIDTH - 1, y + KEY_HEIGHT - 1, GRAY);
            
            // 设置按键文字
            key_str[0] = row[j];
            if (key_str[0] == '<') key_str[0] = 0x7F;
            key_str[1] = '\0';
            
            POINT_COLOR = BLACK;
            LCD_ShowString(x + (KEY_WIDTH - 8) / 2, y + (KEY_HEIGHT - 16) / 2, 16, 16, 16, (u8 *)key_str);
        }
    }
    
    // 单独绘制OK确认按钮
    LCD_DrawRectangle(OK_BTN_X, OK_BTN_Y, OK_BTN_X + OK_BTN_W, OK_BTN_Y + OK_BTN_H);
    LCD_Fill(OK_BTN_X + 1, OK_BTN_Y + 1, OK_BTN_X + OK_BTN_W - 1, OK_BTN_Y + OK_BTN_H - 1, GREEN);
    LCD_ShowString(OK_BTN_X + (OK_BTN_W - 48)/2, OK_BTN_Y + (OK_BTN_H - 24)/2, 48, 24, 24, "OK");
}

// 切换为OV2640模式
void sw_ov2640_mode(void)
{
    OV2640_PWDN = 0; // OV2640 Power Up
    // GPIOC8/9/11切换为 DCMI接口
    GPIO_AF_Set(GPIOC, 8, 13);  // PC8,AF13  DCMI_D2
    GPIO_AF_Set(GPIOC, 9, 13);  // PC9,AF13  DCMI_D3
    GPIO_AF_Set(GPIOC, 11, 13); // PC11,AF13 DCMI_D4
}

// 切换为SD卡模式
void sw_sdcard_mode(void)
{
    OV2640_PWDN = 1; // OV2640 Power Down
    // GPIOC8/9/11切换为 SDIO接口
    GPIO_AF_Set(GPIOC, 8, 12);  // PC8,AF12
    GPIO_AF_Set(GPIOC, 9, 12);  // PC9,AF12
    GPIO_AF_Set(GPIOC, 11, 12); // PC11,AF12
}

// 保存用户信息到SD卡
FRESULT save_to_sd(u8 person, u8 *name, u8 *phone)
{
    u8 Name[MAX_NAME_LEN]; 
    FRESULT res;
    FIL file;
    UINT bw;
    char filename[30];
    char line[100];

    // 安全复制name到Name缓冲区
    strncpy((char*)Name, (char*)name, MAX_NAME_LEN - 1);
    Name[MAX_NAME_LEN - 1] = '\0';

    // 切换到SD卡模式前检查挂载状态
    if(fs[0]->fs_type == 0) { // 文件系统未挂载
        res = f_mount(fs[0], "0:", 1);
        if(res != FR_OK) {
            printf("重新挂载SD卡失败! 错误代码: %d\r\n", res);
            return res;
        }
    }
    
    sw_sdcard_mode();
    
    // 创建infor目录（如果不存在）
    res = f_mkdir("0:/infor");
    if (res != FR_OK && res != FR_EXIST) {
        printf("创建目录失败! 错误代码: %d\r\n", res);
        return res;
    }
    
    // 生成文件名
    sprintf(filename, "0:/infor/person_%02d.txt", person);
    
    // 打开文件（创建或覆盖）
    res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if(res != FR_OK) {
        printf("打开文件失败! 错误代码: %d\r\n", res);
        return res;
    }
    
    // 构建完整的行数据
    sprintf(line, "Name: %s\r\nPhone: %s\r\n", name, phone);
    
    // 写入完整的一行
    res = f_write(&file, line, strlen(line), &bw);
    if(res != FR_OK || bw != strlen(line)) {
        printf("写入文件失败! 错误代码: %d, 写入字节: %d\r\n", res, bw);
        f_close(&file);
        return res;
    }
    
    // 关闭文件
    res = f_close(&file);
    if(res != FR_OK) {
        printf("关闭文件失败! 错误代码: %d\r\n", res);
        return res;
    }
    
    // 确保数据写入物理设备
    res = f_sync(&file);
    if(res != FR_OK) {
        printf("同步文件失败! 错误代码: %d\r\n", res);
        return res;
    }
    
    // 切换回摄像头模式
    sw_ov2640_mode();
    
    return res;
}


void add_face_UI(u8 person)
{
    u8 key;
    u8 res;
		int i;
    FRESULT res1;
    LCD_Clear(WHITE);
    POINT_COLOR=BLACK;
    
    // 显示输入界面
    LCD_ShowString(50, 100, 200, 60, 24, "NAME:");
    LCD_ShowString(50, 150, 200, 60, 24, "TELEPHONE:");
    frec_show_picture(400,100,30,40,1.0,pixdatabuf);
    
    // 申请内存
    namedatabuf = mymalloc(SRAMIN, MAX_NAME_LEN);
    phonedatabuf = mymalloc(SRAMIN, MAX_PHONE_LEN);
    if(!namedatabuf || !phonedatabuf) {
        printf("内存分配失败! namedatabuf=%p, phonedatabuf=%p\r\n", namedatabuf, phonedatabuf);
        return;
    }
    memset(namedatabuf, 0, MAX_NAME_LEN);
    memset(phonedatabuf, 0, MAX_PHONE_LEN);
    
    FaceDB_Init();
    
    // 初始化输入缓冲区
    name_input_pos = 0;
    phone_input_pos = 0;
    name_selected = 1; // 默认选中姓名
    
    // 初始显示
    show_input_boxes();
    keyboard_UI();
    
    printf("进入人脸添加界面，人员编号: %d\r\n", person);
    
    while (1) {
        tp_dev.scan(0);         
        
        if(tp_dev.sta & TP_PRES_DOWN) {
            // 检查输入框选择
            if(tp_dev.x[0]>50 && tp_dev.y[0]>100 && tp_dev.x[0]<250 && tp_dev.y[0]<160) {
                name_selected = 1;
                show_input_boxes();
                keyboard_UI();
                printf("选中姓名输入框\r\n");
            }
            else if(tp_dev.x[0]>50 && tp_dev.y[0]>150 && tp_dev.x[0]<250 && tp_dev.y[0]<210) {
                name_selected = 0;
                show_input_boxes();
                keyboard_UI();
                printf("选中电话输入框\r\n");
            }
        } 
        else {
            delay_ms(10);
        }
        
        // 处理键盘输入
        key = check_keyboard_input();
        if(key) {
            printf("按键按下: ");
            if(key == BACKSPACE_KEY) {
                printf("退格键\r\n");
            } else if(key == OK_KEY) {
                printf("确认键\r\n");
            } else {
                printf("字符 '%c'\r\n", key);
            }
            
            if(key == OK_KEY) { // 确认按钮
                printf("尝试添加人脸到数据库...\r\n");
                printf("姓名: %s, 电话: %s\r\n", namedatabuf, phonedatabuf);
                printf("剩余内存: SRAMIN=%d, SRAMEX=%d\r\n", my_mem_perused(SRAMIN), mem_perused(SRAMEX));
								// 在调用前检查图像数据
								printf("图像数据指针: %p, 前10个像素值:\r\n", pixdatabuf);
								for(i=0; i<10; i++) {
										printf("%04X ", pixdatabuf[i]);
								}
								printf("\r\n");
                // 添加人脸到数据库
                res = atk_frec_add_a_face(pixdatabuf, &person);
								switch(res) {
										case 0: printf("添加成功\r\n"); break;
										case 1: printf("内存错误\r\n"); break;
										case 2: printf("人脸模板已满\r\n"); break;
										case 3: printf("数据存储/读取失败\r\n"); 
														// 检查SD卡状态和文件系统
														break;
										case 4: printf("没有可用模板\r\n"); break;
										case 5: printf("无法识别人脸\r\n"); break;
										default: printf("未知错误\r\n");
								}
                if(res == 0) {
                    sprintf((char *)msgbuf, "添加成功,编号:%02d   ", person);
                    printf("人脸添加成功! 人员编号: %d\r\n", person);
                    
                    atk_frec_load_data_model(); // 重新加载所有识别模型
                    printf("重新加载人脸识别模型...\r\n");
                    // 检查SD卡状态
										if(f_mount(fs[0], "0:", 0) != FR_OK) { // 快速检查而不重新挂载
												printf("SD卡未就绪，尝试重新挂载...\r\n");
												res1 = f_mount(fs[0], "0:", 1); // 强制重新挂载
												if(res1 != FR_OK) {
														LCD_ShowString(50, 200, 200, 24, 24, "SD卡错误!");
														printf("SD卡挂载失败! 错误代码: %d\r\n", res1);
														delay_ms(1000);
														return ;
												}
										}
                    // 保存到SD卡
                    printf("尝试保存数据到SD卡...\r\n");
                    res1 = save_to_sd(person, namedatabuf, phonedatabuf);
                    if(res1 == FR_OK) {
                        LCD_ShowString(50, 200, 200, 24, 24, "保存成功!");
                        printf("SD卡保存成功!\r\n");
                    } 
                    else {
                        LCD_ShowString(50, 200, 200, 24, 24, "保存失败!");
                        printf("SD卡保存失败! 错误代码: %d\r\n", res1);
                    }
                    delay_ms(1000);
                }
                else {
                    sprintf((char *)msgbuf, "添加失败,错误代码:%02d", res);
                    printf("人脸添加失败! 错误代码: %d\r\n", res);
                }
                
                // 清空输入
                memset(namedatabuf, 0, 20);
                memset(phonedatabuf, 0, 20);
                
                name_input_pos = 0;
                phone_input_pos = 0;
                name_selected = 1;
                
                // 释放内存并返回
                myfree(SRAMIN, namedatabuf);
                myfree(SRAMIN, phonedatabuf);
                printf("释放内存并退出添加界面\r\n");
                
                // 停止摄像头传输并返回主界面
                DCMI_Stop();
                sw_sdcard_mode();
                return;
            }  
            else {
                if(name_selected) {
                    handle_name_input(key);
                    printf("姓名输入: %s\r\n", namedatabuf);
                }
                else {
                    handle_phone_input(key);
                    printf("电话输入: %s\r\n", phonedatabuf);
                }
            }
        }
    }
}

void recognize_face_UI(u8 person,u8 res) {
		int i;
    FRESULT fr;
    FIL file;
    UINT br;
    char filename[30];
    char line[100];
    char name[20] = {0};
    char phone[20] = {0};
    u8 found = 0;
    
    // 清屏并显示识别结果
    LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
    
    // 显示人脸图像
    frec_show_picture(100, 50, 30, 40, 1.0, pixdatabuf);
		//printf("图像数据指针: %p, 前10个像素值:\n", pixdatabuf);
		//for(i=0; i<10; i++) {
		//		printf("%04X ", pixdatabuf[i]);
		//}
		//printf("\n");
    //printf("开始人脸识别...\n");
    // 识别结果
		delay_ms(100);
		printf("识别结果: res=%d, person=%d\n", res, person); // 调试输出
    if (res == ATK_FREC_MODEL_DATA_ERR) {
        LCD_ShowString(50, 200, 200, 24, 24, "没有可用模板!");
        LCD_ShowString(50, 230, 200, 24, 24, "请先添加用户");
    } 
    else if (res == ATK_FREC_UNREC_FACE_ERR) {
        LCD_ShowString(50, 200, 200, 24, 24, "无法识别人脸!");
        LCD_ShowString(50, 230, 200, 24, 24, "请重试");
    } 
    else {
				DCMI_Stop();
        // 从SD卡读取用户信息
        sw_sdcard_mode();
        
        // 生成文件名
        sprintf(filename, "0:/infor/person_%02d.txt", person);
        
				printf("尝试打开文件: %s\n", filename); // 调试输出
        // 打开文件
        fr = f_open(&file, filename, FA_READ);
        if (fr == FR_OK) {
						printf("文件打开成功\n");
            // 读取文件内容
            while (f_gets(line, sizeof(line), &file)) {
                if (strstr(line, "Name:")) {
                    sscanf(line, "Name: %19[^\r\n]", name);
                    found = 1;
										printf("找到姓名: %s\n", name);
                } 
                else if (strstr(line, "Phone:")) {
                    sscanf(line, "Phone: %19[^\r\n]", phone);
										printf("找到电话: %s\n", phone);
                }
            }
            f_close(&file);
            
            if (found) {
                // 显示用户信息
                LCD_ShowString(50, 200, 200, 24, 24, "识别成功:");
                LCD_ShowString(50, 230, 200, 24, 24, "编号:");
                LCD_ShowNum(110, 230, person, 2, 24);
                LCD_ShowString(50, 260, 200, 24, 24, "姓名:");
                LCD_ShowString(110, 260, 200, 24, 24, (u8*)name);
                LCD_ShowString(50, 290, 200, 24, 24, "电话:");
                LCD_ShowString(110, 290, 200, 24, 24, (u8*)phone);
            } else {
								printf("文件打开失败: %d\n", fr);
                LCD_ShowString(50, 200, 200, 24, 24, "识别成功但未找到");
                LCD_ShowString(50, 230, 200, 24, 24, "用户信息!");
            }
        } else {
						printf("文件打开失败: %d\n", fr);
            LCD_ShowString(50, 200, 200, 24, 24, "识别成功但未找到");
            LCD_ShowString(50, 230, 200, 24, 24, "用户信息文件!");
        }
        
        // 切换回摄像头模式
        sw_ov2640_mode();
    }
    
    // 添加OK按钮
    LCD_DrawRectangle(200, 350, 300, 390);
    LCD_Fill(201, 351, 299, 389, GREEN);
    LCD_ShowString(220, 360, 80, 24, 24, "OK");
    
    // 等待用户点击OK
    while(1) {
        tp_dev.scan(0);
        if(tp_dev.sta & TP_PRES_DOWN) {
            if(tp_dev.x[0] > 200 && tp_dev.x[0] < 300 && 
               tp_dev.y[0] > 350 && tp_dev.y[0] < 390) {
                // 按钮按下效果
                LCD_Fill(201, 351, 299, 389, BLUE);
                LCD_ShowString(220, 360, 80, 24, 24, "OK");
                delay_ms(100);
                LCD_Fill(201, 351, 299, 389, GREEN);
                LCD_ShowString(220, 360, 80, 24, 24, "OK");
                
                return; // 返回上级界面
            }
        }
        delay_ms(10);
    }
}

void Init_UI(){
	LCD_Clear(WHITE);
	POINT_COLOR=BLACK;
	LCD_DrawRectangle(95,145,405,255);
	LCD_Fill(100,150,400,250,BLUE);//
	LCD_DrawRectangle(95,345,405,455);
	LCD_Fill(100,350,400,450,BLUE);//
	LCD_DrawRectangle(95,545,405,655);
	LCD_Fill(100,550,400,650,BLUE);//
	POINT_COLOR=YELLOW;
	LCD_ShowString(200, 200, 200, 60, 24, "ADD USER");
	LCD_ShowString(175, 400, 200, 60, 24, "RECOGNIZE USER");
	LCD_ShowString(175, 600, 200, 60, 24, "DELETE USER");
	
}

void Init(){
		Stm32_Clock_Init(336, 8, 2, 7);   // 设置时钟,168Mhz
    delay_init(168);                  // 延时初始化
    uart_init(84, 115200);            // 初始化串口波特率为115200
    LED_Init();                       // 初始化LED
    usmart_dev.init(84);              // 初始化USMART
    TIM3_Int_Init(100 - 1, 8400 - 1); // 10Khz计数,10ms中断一次
    LCD_Init();                       // LCD初始化
    FSMC_SRAM_Init();                 // 初始化外部SRAM.
    BEEP_Init();                      // 蜂鸣器初始化
    KEY_Init();                       // 按键初始化
		tp_dev.init();				//触摸屏初始化
    W25QXX_Init();                    // 初始化W25Q128
    my_mem_init(SRAMIN);              // 初始化内部内存池
    my_mem_init(SRAMEX);              // 初始化内部内存池
    my_mem_init(SRAMCCM);             // 初始化CCM内存池
    exfuns_init();                    // 为fatfs相关变量申请内存
    f_mount(fs[0], "0:", 1);          // 挂载SD卡
    f_mount(fs[1], "1:", 1);          // 挂载SPI FLASH
		if(f_mount(fs[0], "0:", 1) != FR_OK) {
				printf("SD卡挂载失败!\r\n");
		} else {
				printf("SD卡挂载成功!\r\n");
		}
		
	  POINT_COLOR = RED;
		while (font_init()) // 检查字库
    {
        LCD_ShowString(30, 50, 200, 16, 16, "Font Error!");
        delay_ms(200);
        LCD_Fill(30, 50, 240, 66, WHITE); // 清除显示
        delay_ms(200);
    }
    
    Show_Str(30, 150, 200, 16, "2025年6月20日", 16, 0);
    while (SD_Init()) // 检查SD卡
    {
        Show_Str(30, 190, 240, 16, "SD Card Error!", 16, 0);
        delay_ms(200);
        LCD_Fill(30, 190, 239, 206, WHITE);
        delay_ms(200);
    }
    while (OV2640_Init()) // 初始化OV2640
    {
        Show_Str(30, 190, 240, 16, "OV7670 错误!", 16, 0);
        delay_ms(200);
        LCD_Fill(30, 190, 239, 206, WHITE);
        delay_ms(200);
    }
    pixdatabuf = mymalloc(SRAMIN, ATK_GABOR_IMG_WID * ATK_GABOR_IMG_HEI * 2); // 申请内存
    Show_Str(30, 190, 200, 16, "OV2640 正常", 16, 0);
    delay_ms(2000);
    OV2640_RGB565_Mode();                                // RGB565输出
    OV2640_ImageWin_Set((1600 - 900) / 2, 0, 900, 1200); // 设置输出尺寸为:900*1200,3:4比例
    DCMI_Init();                                         // DCMI配置
    DCMI_DMA_Init((u32)&LCD->LCD_RAM, 0, 1, 1, 0);       // DCMI DMA配置
    Init_UI();
		//add_face_UI(1);
}



//////////////////////////////////////////////////////////////////////////////////////////
// LCD显示区域限制参数
u16 face_offx, face_offy;
u16 face_xsize, face_ysize;

u8 fontsize = 12; // 字体大小

// 设置图像到屏幕最中心.
void set_image_center(void)
{
    face_offx = 0;
    face_offy = 0;
    face_xsize = lcddev.width;
    face_ysize = lcddev.height;
    if (lcddev.id == 0X1963 || lcddev.id == 0X5510)
    {
        face_offy = 80;
        face_ysize = 640;
        fontsize = 24;
    }
    else if (lcddev.id == 0X5310)
    {
        face_offx = 10;
        face_offy = 40;
        face_xsize = 300;
        face_ysize = 400;
        fontsize = 16;
    }
    else
        fontsize = 12;
    LCD_Set_Window(face_offx, face_offy, face_xsize, face_ysize); // 设置开窗口.
}

// 加载一个简单界面
// fsize:字体大小
void frec_load_ui(u8 fsize)
{
    if (fsize == 16)
    {
        Show_Str(10, 2, 310, fsize, "KEY0:开始识别  KEY2:删除所有模板", fsize, 1);
        Show_Str(10, 4 + 16, 310, fsize, "WK_UP:添加人脸模板", fsize, 1);
    }
    else if (fsize == 24)
    {
        Show_Str(10, 10, 470, fsize, "KEY0:开始识别  KEY2:删除所有模板", fsize, 1);
        Show_Str(10, 20 + 24, 470, fsize, "WK_UP:添加人脸模板", fsize, 1);
    }
}
// 显示提示信息
// str:要显示的字符串
// line:第几行;0,第一行;1,第二行;其他,非法.
// fsize:字体大小
void frec_show_msg(u8 *str, u8 line)
{
    if (line > 1)
        return;
    if (lcddev.width == 240)
    {
        Show_Str(10, lcddev.height - (2 - line) * fontsize - (2 - line) * 5, lcddev.width, fontsize, str, fontsize, 0);
    }
    else
    {
        Show_Str(10, lcddev.height - (2 - line) * fontsize - (2 - line) * (face_offy - fontsize * 2) / 3, lcddev.width, fontsize, str, fontsize, 1);
    }
}


int add_face()
{
	u8 i;
	u8 key;
	u8 res;
	u8 person;
	LCD_Clear(BLACK);
	set_image_center();     // 设置到屏幕正中央
	Show_Str(10, 2, 50, 24, "BACK", 24, 1);//显示清屏区域
	//frec_load_ui(fontsize); // 显示GUI
	OV2640_OutSize_Set(face_xsize, face_ysize);
	sw_sdcard_mode();                // SD卡模式
	res = atk_frec_initialization(); // 初始化人脸识别
	if(res) {
    printf("人脸识别库初始化失败! 错误代码: %d\r\n", res);
} else {
    printf("人脸识别库初始化成功!\r\n");
}
	sw_ov2640_mode(); // 2640模式
	DCMI_Start();     // 启动传输
	while(1){
		delay_ms(10);
		key = KEY_Scan(0);
		tp_dev.scan(0);
		if(tp_dev.sta&TP_PRES_DOWN)			//触摸屏被按下
				{	
					if(tp_dev.x[0]>10&&tp_dev.y[0]>2&&tp_dev.x[0]<60&&tp_dev.y[0]<20)
					{	
						 NVIC_SystemReset();
					}
				}
		//else delay_ms(10);	//没有按键按下的时候 	
		if(key){
			DCMI_Stop();      // 停止传输
      sw_sdcard_mode(); // SD卡模式
			switch (key)
						{
						case WKUP_PRES:                                                            // 添加一个人像进入数据库
								frec_get_image_data(pixdatabuf, face_offx, face_offy, face_xsize, 30); // 读取图像数据
								add_face_UI(person);
								NVIC_SystemReset();
								break;
						default:
								break;
						}
						sw_ov2640_mode(); // 2640模式
						DCMI_Start();     // 启动传输
		}
		if(res==0)
		delay_ms(10);
		i++;
		if (i == 20) // DS0闪烁.
		{
				i = 0;
				LED0 = !LED0;
		}
	}
}

void recognize_face(){
	u8 i;
	u8 key;
	u8 res;
	u8 person;
	LCD_Clear(BLACK);
	set_image_center();     // 设置到屏幕正中央
	Show_Str(10, 2, 50, 24, "BACK", 24, 1);//显示清屏区域
	//frec_load_ui(fontsize); // 显示GUI
	OV2640_OutSize_Set(face_xsize, face_ysize);
	sw_sdcard_mode();                // SD卡模式
	res = atk_frec_initialization(); // 初始化人脸识别
	if(res) {
    printf("人脸识别库初始化失败! 错误代码: %d\r\n", res);
} else {
    printf("人脸识别库初始化成功!\r\n");
}
	sw_ov2640_mode(); // 2640模式
	DCMI_Start();     // 启动传输
	while(1){
		delay_ms(10);
		key = KEY_Scan(0);
		tp_dev.scan(0);
		if(tp_dev.sta&TP_PRES_DOWN)			//触摸屏被按下
				{	
					if(tp_dev.x[0]>10&&tp_dev.y[0]>2&&tp_dev.x[0]<60&&tp_dev.y[0]<20)
					{	
						 NVIC_SystemReset();
					}
				}
		//else delay_ms(10);	//没有按键按下的时候 	
		if(key){
			DCMI_Stop();      // 停止传输
      sw_sdcard_mode(); // SD卡模式
			switch (key)
						{
						case WKUP_PRES:                                                          
								frec_get_image_data(pixdatabuf, face_offx, face_offy, face_xsize, 30); // 读取图像数据
								res = atk_frec_recognition_face(pixdatabuf, &person);
								recognize_face_UI(person,res);
								NVIC_SystemReset();
								break;
						default:
								break;
						}
						sw_ov2640_mode(); // 2640模式
						DCMI_Start();     // 启动传输
		}
		if(res==0)
		delay_ms(10);
		i++;
		if (i == 20) // DS0闪烁.
		{
				i = 0;
				LED0 = !LED0;
		}
	}
}

void delete_face()
{
    u8 i, j;
    u8 res;
    u8 selected_person = 0xFF; // 初始没有选中任何人
    FRESULT fr1;
    FIL file1;
    UINT br1;
    char filename[30];
    char line[100];
    char name[20]={0};
    u8 name_count = 0;
    char name_list[MAX_FACES][20] = {0}; // 存储姓名列表
    char person_ids[MAX_FACES] = {0};    // 存储对应的person ID
		OV2640_OutSize_Set(face_xsize, face_ysize);
		sw_sdcard_mode();                // SD卡模式
		res = atk_frec_initialization(); // 初始化人脸识别
		exfuns_init();                    // 为fatfs相关变量申请内存
    f_mount(fs[0], "0:", 1);          // 挂载SD卡
    f_mount(fs[1], "1:", 1);          // 挂载SPI FLASH
		if(f_mount(fs[0], "0:", 1) != FR_OK) {
				printf("SD卡挂载失败!\r\n");
		} else {
				printf("SD卡挂载成功!\r\n");
		}
		//清屏显示识别结果
		LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
		delay_ms(100);
    // 切换到SD卡模式
		DCMI_Stop();
    sw_sdcard_mode();
    // ==================== SD卡和目录检查 ====================
    printf("\n=== 开始删除流程 ===\n");
    for(i=0;i<MAX_FACES;i++){
			// 生成文件名
        sprintf(filename, "0:/infor/person_%02d.txt", i);
				printf("尝试打开文件: %s\n", filename); // 调试输出
				// 打开文件
        fr1 = f_open(&file1, filename, FA_READ);
				if (fr1==FR_OK){
					printf("文件打开成功\n");
					// 读取文件内容
					while (f_gets(line, sizeof(line), &file1)) {
							if (strstr(line, "Name:")) {
									sscanf(line, "Name: %19[^\r\n]", name);
									printf("找到姓名: %s\n", name);
									name_count++;
									strcpy((char*)name_list[name_count], name);
									person_ids[name_count] = i;
							} 
					}
				}
				else{
					printf("文件打开失败: %d\n", fr1);
				}
				f_close(&file1);
		}
    
    // 清屏并显示标题
    LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
    LCD_ShowString(50, 20, 200, 24, 24, "DELETE USER");
    LCD_ShowString(50, 50, 200, 16, 16, "Select a user to delete:");
    
    // 绘制分割线
    LCD_DrawLine(50, 70, 430, 70);
		
    // ==================== 用户界面显示 ====================
    // 如果没有找到用户
    if(name_count == 0) {
        LCD_ShowString(50, 100, 200, 24, 24, "未找到用户!");
        LCD_ShowString(50, 130, 200, 16, 16, "点击任意位置返回");
        
        while(1) {
            tp_dev.scan(0);
            if(tp_dev.sta & TP_PRES_DOWN) {
                return;
            }
            delay_ms(10);
        }
    }
    
    // 显示用户列表
    for(i = 0; i < name_count; i++) {
        // 绘制列表项背景
        LCD_Fill(50, 80 + i*40, 430, 80 + (i+1)*40 - 2, WHITE);
        POINT_COLOR = BLACK;
        LCD_DrawRectangle(50, 80 + i*40, 430, 80 + (i+1)*40 - 2);
        
        // 显示用户名
        LCD_ShowString(60, 85 + i*40, 200, 24, 24, name_list[i]);
        
        // 显示用户ID
        LCD_ShowString(350, 85 + i*40, 50, 24, 24, "ID:");
        LCD_ShowNum(380, 85 + i*40, person_ids[i], 2, 24);
    }
    
    // 绘制底部分割线
    POINT_COLOR = BLACK;
    LCD_DrawLine(50, 80 + name_count*40, 430, 80 + name_count*40);
    
    // 绘制确认按钮
    POINT_COLOR = BLACK;
    LCD_DrawRectangle(150, 80 + name_count*40 + 10, 330, 80 + name_count*40 + 50);
    LCD_Fill(151, 80 + name_count*40 + 11, 329, 80 + name_count*40 + 49, RED);
    LCD_ShowString(210, 80 + name_count*40 + 20, 80, 24, 24, "DELETE");
    
    // ==================== 主交互循环 ====================
    while(1) {
        tp_dev.scan(0);
        
        if(tp_dev.sta & TP_PRES_DOWN) {
            // 检查是否点击了用户列表项
            for(i = 0; i < name_count; i++) {
                if(tp_dev.x[0] >= 50 && tp_dev.x[0] <= 430 && 
                   tp_dev.y[0] >= 80 + i*40 && tp_dev.y[0] <= 80 + (i+1)*40) {
                    // 取消之前的选择
                    if(selected_person != 0xFF) {
                        j = selected_person;
                        LCD_Fill(50, 80 + j*40, 430, 80 + (j+1)*40 - 2, WHITE);
                        POINT_COLOR = BLACK;
                        LCD_DrawRectangle(50, 80 + j*40, 430, 80 + (j+1)*40 - 2);
                        LCD_ShowString(60, 85 + j*40, 200, 24, 24, name_list[j]);
                        LCD_ShowString(350, 85 + j*40, 50, 24, 24, "ID:");
                        LCD_ShowNum(380, 85 + j*40, person_ids[j], 2, 24);
                    }
                    
                    // 设置新的选择
                    selected_person = i;
                    LCD_Fill(50, 80 + i*40, 430, 80 + (i+1)*40 - 2, BLUE);
                    POINT_COLOR = BLACK;
                    LCD_DrawRectangle(50, 80 + i*40, 430, 80 + (i+1)*40 - 2);
                    LCD_ShowString(60, 85 + i*40, 200, 24, 24, name_list[i]);
                    LCD_ShowString(350, 85 + i*40, 50, 24, 24, "ID:");
                    LCD_ShowNum(380, 85 + i*40, person_ids[i], 2, 24);
                    
                    // 显示提示信息
                    LCD_Fill(50, 80 + name_count*40 + 60, 430, 80 + name_count*40 + 100, WHITE);
                    LCD_ShowString(50, 80 + name_count*40 + 60, 200, 24, 24, "已选择: ");
                    LCD_ShowString(150, 80 + name_count*40 + 60, 200, 24, 24, name_list[i]);
                    
                    break;
                }
            }
            
            // 检查是否点击了删除按钮
            if(tp_dev.x[0] >= 150 && tp_dev.x[0] <= 330 && 
               tp_dev.y[0] >= 80 + name_count*40 + 10 && 
               tp_dev.y[0] <= 80 + name_count*40 + 50) {
                if(selected_person != 0xFF) {
                    // 按钮按下效果
                    LCD_Fill(151, 80 + name_count*40 + 11, 329, 80 + name_count*40 + 49, BLUE);
                    LCD_ShowString(210, 80 + name_count*40 + 20, 80, 24, 24, "DELETE");
                    delay_ms(100);
                    LCD_Fill(151, 80 + name_count*40 + 11, 329, 80 + name_count*40 + 49, RED);
                    LCD_ShowString(210, 80 + name_count*40 + 20, 80, 24, 24, "DELETE");
                    
                    // 切换到SD卡模式
                    sw_sdcard_mode();
                    
                    // 删除人脸模板
                    printf("正在删除人脸模板 %d...", person_ids[selected_person]);
                    res = atk_frec_delete_data(person_ids[selected_person]);
                    if(res == 0) {
                        printf("成功\n");
                        LCD_ShowString(50, 80 + name_count*40 + 90, 200, 24, 24, "模板已删除!");
                    } else {
                        printf("失败! 错误代码: %d\n", res);
                        LCD_ShowString(50, 80 + name_count*40 + 90, 200, 24, 24, "模板删除失败!");
                    }
                    
                    // 删除用户信息文件
                    sprintf(filename, "0:/infor/person_%02d.txt", person_ids[selected_person]);
                    printf("正在删除文件 %s...", filename);
                    fr1 = f_unlink(filename);
                    if(fr1 == FR_OK) {
                        printf("成功\n");
                        LCD_ShowString(50, 80 + name_count*40 + 120, 200, 24, 24, "文件已删除!");
                    } else {
                        printf("失败! 错误代码: %d\n", fr1);
                        LCD_ShowString(50, 80 + name_count*40 + 120, 200, 24, 24, "文件删除失败!");
                    }
                    
                    // 重新加载人脸识别模型
                    printf("重新加载人脸模型...");
                    atk_frec_load_data_model();
                    printf("完成\n");
                    
                    // 切换回摄像头模式
                    sw_ov2640_mode();
                    
                    // 延迟显示结果
                    delay_ms(1500);
                    
                    // 返回主界面
                    return;
                } else {
                    // 没有选中任何人
                    LCD_Fill(50, 80 + name_count*40 + 60, 430, 80 + name_count*40 + 100, WHITE);
                    LCD_ShowString(50, 80 + name_count*40 + 60, 200, 24, 24, "请先选择用户!");
                    delay_ms(1000);
                    LCD_Fill(50, 80 + name_count*40 + 60, 430, 80 + name_count*40 + 100, WHITE);
                }
            }
        }
        
        delay_ms(10);
    }

exit_fail:
    // 错误处理
    delay_ms(2000);
    sw_ov2640_mode();
    return;
}

int main(void)
{
    Init();
    while (1)
    {
        tp_dev.scan(0); 		 
				if(tp_dev.sta&TP_PRES_DOWN)			//触摸屏被按下
				{	
					if(tp_dev.x[0]>100&&tp_dev.y[0]>150&&tp_dev.x[0]<400&&tp_dev.y[0]<250)
					{	
						 add_face();
					}
					if(tp_dev.x[0]>100&&tp_dev.y[0]>350&&tp_dev.x[0]<400&&tp_dev.y[0]<450)
					{	
						 recognize_face();
					}
					if(tp_dev.x[0]>100&&tp_dev.y[0]>550&&tp_dev.x[0]<400&&tp_dev.y[0]<650)
					{	
						 delete_face();
					}
				}else delay_ms(10);	//没有按键按下的时候 	
     
    }
}
