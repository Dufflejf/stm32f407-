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
#define KEY_WIDTH   40      // �������
#define KEY_HEIGHT  40      // �����߶�
#define KEY_MARGIN  5       // �������
#define KEY_ROW1_Y  280     // ��һ�Ű�����Y����
#define KEY_ROW2_Y  330     // �ڶ��Ű�����Y����
#define KEY_ROW3_Y  380     // �����Ű�����Y����
#define KEY_ROW4_Y  430     // �����Ű�����Y����
// ���̲���
const char *keyboard[4] = {
    "1234567890",    // ��һ��
    "qwertyuiop",    // �ڶ���
    "asdfghjkl",     // ������
    "zxcvbnm<--"     // ������
};

#define OK_BTN_X 100
#define OK_BTN_Y 480
#define OK_BTN_W 120
#define OK_BTN_H 40
#define OK_KEY '#' 

// ���������
#define BACKSPACE_KEY '~'  // ���˼�������

#define MAX_NAME_LEN 16    // ������󳤶�
#define MAX_PHONE_LEN 16   // �绰��󳤶�
#define MAX_FACES 50      // �����������

typedef struct {
    u8 id;                 // �������
    u16 name[MAX_NAME_LEN]; // ����
    u16 phone[MAX_PHONE_LEN]; // �绰
} FaceInfo;

// ������Ϣ���ݿ�
FaceInfo faceDatabase[MAX_FACES];
u8 faceCount = 0;          // ��ǰ�洢����������

void FaceDB_Init(void)
{
    faceCount = 0;
    memset(faceDatabase, 0, sizeof(faceDatabase));
}

int reg_time;
u8 ov2640_mode = 0; // ����ģʽ:0,RGB565ģʽ;1,JPEGģʽ
u16 *pixdatabuf; // ͼ�񻺴�
u8 *namedatabuf;//��������
u8 *phonedatabuf;//�绰����

u8 msgbuf[30]; // ��Ϣ������

// ����JPEG����
// ���ɼ���һ֡JPEG���ݺ�,���ô˺���,�л�JPEG BUF.��ʼ��һ֡�ɼ�.
// ��ʾͼƬ��֧�����ţ�
// x,y: ͼƬ��ʾ��ʼ����
// w,h: ԭʼͼƬ��Ⱥ͸߶�
// scale: ���ű���������3��ʾ�Ŵ�3����
// data: ͼƬ���ݻ�����
void frec_show_picture(u16 x, u16 y, u16 w, u16 h, float scale, u16 *data)
{
    u16 i, j;
    u16 scaled_w = w * scale;  // ���ź�Ŀ��
    u16 scaled_h = h * scale;  // ���ź�ĸ߶�
    
    for (i = 0; i < scaled_h; i++)
    {
        for (j = 0; j < scaled_w; j++)
        { 
            // ����ԭʼͼ���Ӧ������λ��
            u16 src_x = j / scale;
            u16 src_y = i / scale;
            
            // ȷ����Խ��
            if(src_x < w && src_y < h)
            {
                LCD_Fast_DrawPoint(x + j, y + i, data[src_y * w + src_x]);
            }
        }
    }
}
// ��ȡԭʼͼƬ����
// dbuf:���ݻ�����
// xoff,yoff:Ҫ��ȡ��ͼ��������ʼ����
// xsize:Ҫ��ȡ��ͼ��������
// width:Ҫ��ȡ�Ŀ��(��߱Ⱥ�Ϊ3:4)
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
    if (ov2640_mode) // ֻ����JPEG��ʽ��,����Ҫ������.
    {
    }
}

// ȫ�ֱ���
u8 name_selected = 1; // Ĭ��ѡ�����������

// ��ʾ�����
void show_input_boxes(void)
{
    // ���ԭ����ʾ����
    LCD_Fill(50, 100, 50 + 200, 100 + 60, WHITE);
    LCD_Fill(50, 150, 50 + 200, 150 + 60, WHITE);
    
    // �������������
    if(name_selected) {
        LCD_DrawRectangle(45, 95, 45 + 210, 95 + 70); // ���
        LCD_DrawRectangle(47, 97, 47 + 206, 97 + 66); // �ڿ�
    }
    LCD_ShowString(50, 100, 200, 60, 24, "NAME:");
    LCD_ShowString(160, 100, 200, 24, 24, (u8 *)namedatabuf);
    
    // ���Ƶ绰�����
    if(!name_selected) {
        LCD_DrawRectangle(45, 145, 45 + 210, 145 + 70); // ���
        LCD_DrawRectangle(47, 147, 47 + 206, 147 + 66); // �ڿ�
    }
    LCD_ShowString(50, 150, 200, 60, 24, "TELEPHONE:");
    LCD_ShowString(160, 150, 200, 24, 24, (u8 *)phonedatabuf);
}

// ȫ�ֱ���
u8 name_input_pos = 0;  // ��ǰ����λ��
u8 phone_input_pos = 0; // ��ǰ����λ��
u8 current_input = 0;   // 0-��������, 1-�绰����

// ����������
// ����: ���µļ�ֵ, 0��ʾû�а�������
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
        
        // ����Ƿ��ڼ�������
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
                            
                            // ��������
                            if(current_key != last_key) {
                                last_key = current_key;
                                last_press_time = current_time;
                                key_repeat = 0;
                            } else {
                                // ������������
                                if(current_time - last_press_time > (key_repeat ? 100000 : 300000)) {
                                    last_press_time = current_time;
                                    key_repeat = 1;
                                } else {
                                    return 0;
                                }
                            }
                            
                            // ������ʾ���µļ�
                            LCD_Fill(key_x + 1, row_y + 1, key_x + KEY_WIDTH - 1, row_y + KEY_HEIGHT - 1, BLUE);
                            
                            // ���ð�������
                            key_str[0] = row[j];
                            if (key_str[0] == '<') key_str[0] = 0x7F;
                            key_str[1] = '\0';
                            
                            POINT_COLOR = BLACK;
                            LCD_ShowString(key_x + (KEY_WIDTH - 8) / 2, row_y + (KEY_HEIGHT - 16) / 2, 16, 16, 16, (u8 *)key_str);
                            
                            delay_ms(50); // ���̸���ʱ��
                            
                            // �ָ���������
                            LCD_Fill(key_x + 1, row_y + 1, key_x + KEY_WIDTH - 1, row_y + KEY_HEIGHT - 1, GRAY);
                            LCD_ShowString(key_x + (KEY_WIDTH - 8) / 2, row_y + (KEY_HEIGHT - 16) / 2, 16, 16, 16, (u8 *)key_str);
                            
                            return current_key;
                        }
                    }
                }
            }
        }
        
        // ����Ƿ�����OK��ť
        if (x >= OK_BTN_X && x <= OK_BTN_X + OK_BTN_W && 
            y >= OK_BTN_Y && y <= OK_BTN_Y + OK_BTN_H) {
            // ��ť����Ч��
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

// ������������
void handle_name_input(u8 key)
{
    if (key == BACKSPACE_KEY) { // ���˼�
        if (name_input_pos > 0) {
            name_input_pos--;
            ((u8 *)namedatabuf)[name_input_pos] = '\0';
        }
    } 
    else if (name_input_pos < MAX_NAME_LEN - 1) { // ��ͨ��
        ((u8 *)namedatabuf)[name_input_pos] = key;
        name_input_pos++;
        ((u8 *)namedatabuf)[name_input_pos] = '\0';
    }
    
    // ������ʾ
    LCD_Fill(160, 100, 160 + 200, 100 + 24, WHITE);
    LCD_ShowString(160, 100, 200, 24, 24, (u8 *)namedatabuf);
}

// ����绰����
void handle_phone_input(u8 key)
{
    if (key == BACKSPACE_KEY) { // ���˼�
        if (phone_input_pos > 0) {
            phone_input_pos--;
            ((u8 *)phonedatabuf)[phone_input_pos] = '\0';
        }
    } 
    else if (phone_input_pos < MAX_PHONE_LEN - 1) { // ��ͨ��
        ((u8 *)phonedatabuf)[phone_input_pos] = key;
        phone_input_pos++;
        ((u8 *)phonedatabuf)[phone_input_pos] = '\0';
    }
    
    // ������ʾ
    LCD_Fill(160, 150, 160 + 200, 150 + 24, WHITE);
    LCD_ShowString(160, 150, 200, 24, 24, (u8 *)phonedatabuf);
}

// �޸ļ���UI����
void keyboard_UI()
{
    u8 i, j;
    u16 x, y;
    u16 start_x, row_y;
    char key_str[2] = {'\0'};
    
    // �������Ű���
    for (i = 0; i < 4; i++) {
        const char *row = keyboard[i];
        u8 row_len = strlen(row);
        
        // ������ʼX����ʹ���̾���
        start_x = (lcddev.width - (row_len * (KEY_WIDTH + KEY_MARGIN) - KEY_MARGIN)) / 2;
        
        for (j = 0; j < row_len; j++) {
            x = start_x + j * (KEY_WIDTH + KEY_MARGIN);
            y = KEY_ROW1_Y + i * (KEY_HEIGHT + KEY_MARGIN);
            
            // ���ư�������
            LCD_DrawRectangle(x, y, x + KEY_WIDTH, y + KEY_HEIGHT);
            LCD_Fill(x + 1, y + 1, x + KEY_WIDTH - 1, y + KEY_HEIGHT - 1, GRAY);
            
            // ���ð�������
            key_str[0] = row[j];
            if (key_str[0] == '<') key_str[0] = 0x7F;
            key_str[1] = '\0';
            
            POINT_COLOR = BLACK;
            LCD_ShowString(x + (KEY_WIDTH - 8) / 2, y + (KEY_HEIGHT - 16) / 2, 16, 16, 16, (u8 *)key_str);
        }
    }
    
    // ��������OKȷ�ϰ�ť
    LCD_DrawRectangle(OK_BTN_X, OK_BTN_Y, OK_BTN_X + OK_BTN_W, OK_BTN_Y + OK_BTN_H);
    LCD_Fill(OK_BTN_X + 1, OK_BTN_Y + 1, OK_BTN_X + OK_BTN_W - 1, OK_BTN_Y + OK_BTN_H - 1, GREEN);
    LCD_ShowString(OK_BTN_X + (OK_BTN_W - 48)/2, OK_BTN_Y + (OK_BTN_H - 24)/2, 48, 24, 24, "OK");
}

// �л�ΪOV2640ģʽ
void sw_ov2640_mode(void)
{
    OV2640_PWDN = 0; // OV2640 Power Up
    // GPIOC8/9/11�л�Ϊ DCMI�ӿ�
    GPIO_AF_Set(GPIOC, 8, 13);  // PC8,AF13  DCMI_D2
    GPIO_AF_Set(GPIOC, 9, 13);  // PC9,AF13  DCMI_D3
    GPIO_AF_Set(GPIOC, 11, 13); // PC11,AF13 DCMI_D4
}

// �л�ΪSD��ģʽ
void sw_sdcard_mode(void)
{
    OV2640_PWDN = 1; // OV2640 Power Down
    // GPIOC8/9/11�л�Ϊ SDIO�ӿ�
    GPIO_AF_Set(GPIOC, 8, 12);  // PC8,AF12
    GPIO_AF_Set(GPIOC, 9, 12);  // PC9,AF12
    GPIO_AF_Set(GPIOC, 11, 12); // PC11,AF12
}

// �����û���Ϣ��SD��
FRESULT save_to_sd(u8 person, u8 *name, u8 *phone)
{
    u8 Name[MAX_NAME_LEN]; 
    FRESULT res;
    FIL file;
    UINT bw;
    char filename[30];
    char line[100];

    // ��ȫ����name��Name������
    strncpy((char*)Name, (char*)name, MAX_NAME_LEN - 1);
    Name[MAX_NAME_LEN - 1] = '\0';

    // �л���SD��ģʽǰ������״̬
    if(fs[0]->fs_type == 0) { // �ļ�ϵͳδ����
        res = f_mount(fs[0], "0:", 1);
        if(res != FR_OK) {
            printf("���¹���SD��ʧ��! �������: %d\r\n", res);
            return res;
        }
    }
    
    sw_sdcard_mode();
    
    // ����inforĿ¼����������ڣ�
    res = f_mkdir("0:/infor");
    if (res != FR_OK && res != FR_EXIST) {
        printf("����Ŀ¼ʧ��! �������: %d\r\n", res);
        return res;
    }
    
    // �����ļ���
    sprintf(filename, "0:/infor/person_%02d.txt", person);
    
    // ���ļ��������򸲸ǣ�
    res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if(res != FR_OK) {
        printf("���ļ�ʧ��! �������: %d\r\n", res);
        return res;
    }
    
    // ����������������
    sprintf(line, "Name: %s\r\nPhone: %s\r\n", name, phone);
    
    // д��������һ��
    res = f_write(&file, line, strlen(line), &bw);
    if(res != FR_OK || bw != strlen(line)) {
        printf("д���ļ�ʧ��! �������: %d, д���ֽ�: %d\r\n", res, bw);
        f_close(&file);
        return res;
    }
    
    // �ر��ļ�
    res = f_close(&file);
    if(res != FR_OK) {
        printf("�ر��ļ�ʧ��! �������: %d\r\n", res);
        return res;
    }
    
    // ȷ������д�������豸
    res = f_sync(&file);
    if(res != FR_OK) {
        printf("ͬ���ļ�ʧ��! �������: %d\r\n", res);
        return res;
    }
    
    // �л�������ͷģʽ
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
    
    // ��ʾ�������
    LCD_ShowString(50, 100, 200, 60, 24, "NAME:");
    LCD_ShowString(50, 150, 200, 60, 24, "TELEPHONE:");
    frec_show_picture(400,100,30,40,1.0,pixdatabuf);
    
    // �����ڴ�
    namedatabuf = mymalloc(SRAMIN, MAX_NAME_LEN);
    phonedatabuf = mymalloc(SRAMIN, MAX_PHONE_LEN);
    if(!namedatabuf || !phonedatabuf) {
        printf("�ڴ����ʧ��! namedatabuf=%p, phonedatabuf=%p\r\n", namedatabuf, phonedatabuf);
        return;
    }
    memset(namedatabuf, 0, MAX_NAME_LEN);
    memset(phonedatabuf, 0, MAX_PHONE_LEN);
    
    FaceDB_Init();
    
    // ��ʼ�����뻺����
    name_input_pos = 0;
    phone_input_pos = 0;
    name_selected = 1; // Ĭ��ѡ������
    
    // ��ʼ��ʾ
    show_input_boxes();
    keyboard_UI();
    
    printf("����������ӽ��棬��Ա���: %d\r\n", person);
    
    while (1) {
        tp_dev.scan(0);         
        
        if(tp_dev.sta & TP_PRES_DOWN) {
            // ��������ѡ��
            if(tp_dev.x[0]>50 && tp_dev.y[0]>100 && tp_dev.x[0]<250 && tp_dev.y[0]<160) {
                name_selected = 1;
                show_input_boxes();
                keyboard_UI();
                printf("ѡ�����������\r\n");
            }
            else if(tp_dev.x[0]>50 && tp_dev.y[0]>150 && tp_dev.x[0]<250 && tp_dev.y[0]<210) {
                name_selected = 0;
                show_input_boxes();
                keyboard_UI();
                printf("ѡ�е绰�����\r\n");
            }
        } 
        else {
            delay_ms(10);
        }
        
        // �����������
        key = check_keyboard_input();
        if(key) {
            printf("��������: ");
            if(key == BACKSPACE_KEY) {
                printf("�˸��\r\n");
            } else if(key == OK_KEY) {
                printf("ȷ�ϼ�\r\n");
            } else {
                printf("�ַ� '%c'\r\n", key);
            }
            
            if(key == OK_KEY) { // ȷ�ϰ�ť
                printf("����������������ݿ�...\r\n");
                printf("����: %s, �绰: %s\r\n", namedatabuf, phonedatabuf);
                printf("ʣ���ڴ�: SRAMIN=%d, SRAMEX=%d\r\n", my_mem_perused(SRAMIN), mem_perused(SRAMEX));
								// �ڵ���ǰ���ͼ������
								printf("ͼ������ָ��: %p, ǰ10������ֵ:\r\n", pixdatabuf);
								for(i=0; i<10; i++) {
										printf("%04X ", pixdatabuf[i]);
								}
								printf("\r\n");
                // ������������ݿ�
                res = atk_frec_add_a_face(pixdatabuf, &person);
								switch(res) {
										case 0: printf("��ӳɹ�\r\n"); break;
										case 1: printf("�ڴ����\r\n"); break;
										case 2: printf("����ģ������\r\n"); break;
										case 3: printf("���ݴ洢/��ȡʧ��\r\n"); 
														// ���SD��״̬���ļ�ϵͳ
														break;
										case 4: printf("û�п���ģ��\r\n"); break;
										case 5: printf("�޷�ʶ������\r\n"); break;
										default: printf("δ֪����\r\n");
								}
                if(res == 0) {
                    sprintf((char *)msgbuf, "��ӳɹ�,���:%02d   ", person);
                    printf("������ӳɹ�! ��Ա���: %d\r\n", person);
                    
                    atk_frec_load_data_model(); // ���¼�������ʶ��ģ��
                    printf("���¼�������ʶ��ģ��...\r\n");
                    // ���SD��״̬
										if(f_mount(fs[0], "0:", 0) != FR_OK) { // ���ټ��������¹���
												printf("SD��δ�������������¹���...\r\n");
												res1 = f_mount(fs[0], "0:", 1); // ǿ�����¹���
												if(res1 != FR_OK) {
														LCD_ShowString(50, 200, 200, 24, 24, "SD������!");
														printf("SD������ʧ��! �������: %d\r\n", res1);
														delay_ms(1000);
														return ;
												}
										}
                    // ���浽SD��
                    printf("���Ա������ݵ�SD��...\r\n");
                    res1 = save_to_sd(person, namedatabuf, phonedatabuf);
                    if(res1 == FR_OK) {
                        LCD_ShowString(50, 200, 200, 24, 24, "����ɹ�!");
                        printf("SD������ɹ�!\r\n");
                    } 
                    else {
                        LCD_ShowString(50, 200, 200, 24, 24, "����ʧ��!");
                        printf("SD������ʧ��! �������: %d\r\n", res1);
                    }
                    delay_ms(1000);
                }
                else {
                    sprintf((char *)msgbuf, "���ʧ��,�������:%02d", res);
                    printf("�������ʧ��! �������: %d\r\n", res);
                }
                
                // �������
                memset(namedatabuf, 0, 20);
                memset(phonedatabuf, 0, 20);
                
                name_input_pos = 0;
                phone_input_pos = 0;
                name_selected = 1;
                
                // �ͷ��ڴ沢����
                myfree(SRAMIN, namedatabuf);
                myfree(SRAMIN, phonedatabuf);
                printf("�ͷ��ڴ沢�˳���ӽ���\r\n");
                
                // ֹͣ����ͷ���䲢����������
                DCMI_Stop();
                sw_sdcard_mode();
                return;
            }  
            else {
                if(name_selected) {
                    handle_name_input(key);
                    printf("��������: %s\r\n", namedatabuf);
                }
                else {
                    handle_phone_input(key);
                    printf("�绰����: %s\r\n", phonedatabuf);
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
    
    // ��������ʾʶ����
    LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
    
    // ��ʾ����ͼ��
    frec_show_picture(100, 50, 30, 40, 1.0, pixdatabuf);
		//printf("ͼ������ָ��: %p, ǰ10������ֵ:\n", pixdatabuf);
		//for(i=0; i<10; i++) {
		//		printf("%04X ", pixdatabuf[i]);
		//}
		//printf("\n");
    //printf("��ʼ����ʶ��...\n");
    // ʶ����
		delay_ms(100);
		printf("ʶ����: res=%d, person=%d\n", res, person); // �������
    if (res == ATK_FREC_MODEL_DATA_ERR) {
        LCD_ShowString(50, 200, 200, 24, 24, "û�п���ģ��!");
        LCD_ShowString(50, 230, 200, 24, 24, "��������û�");
    } 
    else if (res == ATK_FREC_UNREC_FACE_ERR) {
        LCD_ShowString(50, 200, 200, 24, 24, "�޷�ʶ������!");
        LCD_ShowString(50, 230, 200, 24, 24, "������");
    } 
    else {
				DCMI_Stop();
        // ��SD����ȡ�û���Ϣ
        sw_sdcard_mode();
        
        // �����ļ���
        sprintf(filename, "0:/infor/person_%02d.txt", person);
        
				printf("���Դ��ļ�: %s\n", filename); // �������
        // ���ļ�
        fr = f_open(&file, filename, FA_READ);
        if (fr == FR_OK) {
						printf("�ļ��򿪳ɹ�\n");
            // ��ȡ�ļ�����
            while (f_gets(line, sizeof(line), &file)) {
                if (strstr(line, "Name:")) {
                    sscanf(line, "Name: %19[^\r\n]", name);
                    found = 1;
										printf("�ҵ�����: %s\n", name);
                } 
                else if (strstr(line, "Phone:")) {
                    sscanf(line, "Phone: %19[^\r\n]", phone);
										printf("�ҵ��绰: %s\n", phone);
                }
            }
            f_close(&file);
            
            if (found) {
                // ��ʾ�û���Ϣ
                LCD_ShowString(50, 200, 200, 24, 24, "ʶ��ɹ�:");
                LCD_ShowString(50, 230, 200, 24, 24, "���:");
                LCD_ShowNum(110, 230, person, 2, 24);
                LCD_ShowString(50, 260, 200, 24, 24, "����:");
                LCD_ShowString(110, 260, 200, 24, 24, (u8*)name);
                LCD_ShowString(50, 290, 200, 24, 24, "�绰:");
                LCD_ShowString(110, 290, 200, 24, 24, (u8*)phone);
            } else {
								printf("�ļ���ʧ��: %d\n", fr);
                LCD_ShowString(50, 200, 200, 24, 24, "ʶ��ɹ���δ�ҵ�");
                LCD_ShowString(50, 230, 200, 24, 24, "�û���Ϣ!");
            }
        } else {
						printf("�ļ���ʧ��: %d\n", fr);
            LCD_ShowString(50, 200, 200, 24, 24, "ʶ��ɹ���δ�ҵ�");
            LCD_ShowString(50, 230, 200, 24, 24, "�û���Ϣ�ļ�!");
        }
        
        // �л�������ͷģʽ
        sw_ov2640_mode();
    }
    
    // ���OK��ť
    LCD_DrawRectangle(200, 350, 300, 390);
    LCD_Fill(201, 351, 299, 389, GREEN);
    LCD_ShowString(220, 360, 80, 24, 24, "OK");
    
    // �ȴ��û����OK
    while(1) {
        tp_dev.scan(0);
        if(tp_dev.sta & TP_PRES_DOWN) {
            if(tp_dev.x[0] > 200 && tp_dev.x[0] < 300 && 
               tp_dev.y[0] > 350 && tp_dev.y[0] < 390) {
                // ��ť����Ч��
                LCD_Fill(201, 351, 299, 389, BLUE);
                LCD_ShowString(220, 360, 80, 24, 24, "OK");
                delay_ms(100);
                LCD_Fill(201, 351, 299, 389, GREEN);
                LCD_ShowString(220, 360, 80, 24, 24, "OK");
                
                return; // �����ϼ�����
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
		Stm32_Clock_Init(336, 8, 2, 7);   // ����ʱ��,168Mhz
    delay_init(168);                  // ��ʱ��ʼ��
    uart_init(84, 115200);            // ��ʼ�����ڲ�����Ϊ115200
    LED_Init();                       // ��ʼ��LED
    usmart_dev.init(84);              // ��ʼ��USMART
    TIM3_Int_Init(100 - 1, 8400 - 1); // 10Khz����,10ms�ж�һ��
    LCD_Init();                       // LCD��ʼ��
    FSMC_SRAM_Init();                 // ��ʼ���ⲿSRAM.
    BEEP_Init();                      // ��������ʼ��
    KEY_Init();                       // ������ʼ��
		tp_dev.init();				//��������ʼ��
    W25QXX_Init();                    // ��ʼ��W25Q128
    my_mem_init(SRAMIN);              // ��ʼ���ڲ��ڴ��
    my_mem_init(SRAMEX);              // ��ʼ���ڲ��ڴ��
    my_mem_init(SRAMCCM);             // ��ʼ��CCM�ڴ��
    exfuns_init();                    // Ϊfatfs��ر��������ڴ�
    f_mount(fs[0], "0:", 1);          // ����SD��
    f_mount(fs[1], "1:", 1);          // ����SPI FLASH
		if(f_mount(fs[0], "0:", 1) != FR_OK) {
				printf("SD������ʧ��!\r\n");
		} else {
				printf("SD�����سɹ�!\r\n");
		}
		
	  POINT_COLOR = RED;
		while (font_init()) // ����ֿ�
    {
        LCD_ShowString(30, 50, 200, 16, 16, "Font Error!");
        delay_ms(200);
        LCD_Fill(30, 50, 240, 66, WHITE); // �����ʾ
        delay_ms(200);
    }
    
    Show_Str(30, 150, 200, 16, "2025��6��20��", 16, 0);
    while (SD_Init()) // ���SD��
    {
        Show_Str(30, 190, 240, 16, "SD Card Error!", 16, 0);
        delay_ms(200);
        LCD_Fill(30, 190, 239, 206, WHITE);
        delay_ms(200);
    }
    while (OV2640_Init()) // ��ʼ��OV2640
    {
        Show_Str(30, 190, 240, 16, "OV7670 ����!", 16, 0);
        delay_ms(200);
        LCD_Fill(30, 190, 239, 206, WHITE);
        delay_ms(200);
    }
    pixdatabuf = mymalloc(SRAMIN, ATK_GABOR_IMG_WID * ATK_GABOR_IMG_HEI * 2); // �����ڴ�
    Show_Str(30, 190, 200, 16, "OV2640 ����", 16, 0);
    delay_ms(2000);
    OV2640_RGB565_Mode();                                // RGB565���
    OV2640_ImageWin_Set((1600 - 900) / 2, 0, 900, 1200); // ��������ߴ�Ϊ:900*1200,3:4����
    DCMI_Init();                                         // DCMI����
    DCMI_DMA_Init((u32)&LCD->LCD_RAM, 0, 1, 1, 0);       // DCMI DMA����
    Init_UI();
		//add_face_UI(1);
}



//////////////////////////////////////////////////////////////////////////////////////////
// LCD��ʾ�������Ʋ���
u16 face_offx, face_offy;
u16 face_xsize, face_ysize;

u8 fontsize = 12; // �����С

// ����ͼ����Ļ������.
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
    LCD_Set_Window(face_offx, face_offy, face_xsize, face_ysize); // ���ÿ�����.
}

// ����һ���򵥽���
// fsize:�����С
void frec_load_ui(u8 fsize)
{
    if (fsize == 16)
    {
        Show_Str(10, 2, 310, fsize, "KEY0:��ʼʶ��  KEY2:ɾ������ģ��", fsize, 1);
        Show_Str(10, 4 + 16, 310, fsize, "WK_UP:�������ģ��", fsize, 1);
    }
    else if (fsize == 24)
    {
        Show_Str(10, 10, 470, fsize, "KEY0:��ʼʶ��  KEY2:ɾ������ģ��", fsize, 1);
        Show_Str(10, 20 + 24, 470, fsize, "WK_UP:�������ģ��", fsize, 1);
    }
}
// ��ʾ��ʾ��Ϣ
// str:Ҫ��ʾ���ַ���
// line:�ڼ���;0,��һ��;1,�ڶ���;����,�Ƿ�.
// fsize:�����С
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
	set_image_center();     // ���õ���Ļ������
	Show_Str(10, 2, 50, 24, "BACK", 24, 1);//��ʾ��������
	//frec_load_ui(fontsize); // ��ʾGUI
	OV2640_OutSize_Set(face_xsize, face_ysize);
	sw_sdcard_mode();                // SD��ģʽ
	res = atk_frec_initialization(); // ��ʼ������ʶ��
	if(res) {
    printf("����ʶ����ʼ��ʧ��! �������: %d\r\n", res);
} else {
    printf("����ʶ����ʼ���ɹ�!\r\n");
}
	sw_ov2640_mode(); // 2640ģʽ
	DCMI_Start();     // ��������
	while(1){
		delay_ms(10);
		key = KEY_Scan(0);
		tp_dev.scan(0);
		if(tp_dev.sta&TP_PRES_DOWN)			//������������
				{	
					if(tp_dev.x[0]>10&&tp_dev.y[0]>2&&tp_dev.x[0]<60&&tp_dev.y[0]<20)
					{	
						 NVIC_SystemReset();
					}
				}
		//else delay_ms(10);	//û�а������µ�ʱ�� 	
		if(key){
			DCMI_Stop();      // ֹͣ����
      sw_sdcard_mode(); // SD��ģʽ
			switch (key)
						{
						case WKUP_PRES:                                                            // ���һ������������ݿ�
								frec_get_image_data(pixdatabuf, face_offx, face_offy, face_xsize, 30); // ��ȡͼ������
								add_face_UI(person);
								NVIC_SystemReset();
								break;
						default:
								break;
						}
						sw_ov2640_mode(); // 2640ģʽ
						DCMI_Start();     // ��������
		}
		if(res==0)
		delay_ms(10);
		i++;
		if (i == 20) // DS0��˸.
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
	set_image_center();     // ���õ���Ļ������
	Show_Str(10, 2, 50, 24, "BACK", 24, 1);//��ʾ��������
	//frec_load_ui(fontsize); // ��ʾGUI
	OV2640_OutSize_Set(face_xsize, face_ysize);
	sw_sdcard_mode();                // SD��ģʽ
	res = atk_frec_initialization(); // ��ʼ������ʶ��
	if(res) {
    printf("����ʶ����ʼ��ʧ��! �������: %d\r\n", res);
} else {
    printf("����ʶ����ʼ���ɹ�!\r\n");
}
	sw_ov2640_mode(); // 2640ģʽ
	DCMI_Start();     // ��������
	while(1){
		delay_ms(10);
		key = KEY_Scan(0);
		tp_dev.scan(0);
		if(tp_dev.sta&TP_PRES_DOWN)			//������������
				{	
					if(tp_dev.x[0]>10&&tp_dev.y[0]>2&&tp_dev.x[0]<60&&tp_dev.y[0]<20)
					{	
						 NVIC_SystemReset();
					}
				}
		//else delay_ms(10);	//û�а������µ�ʱ�� 	
		if(key){
			DCMI_Stop();      // ֹͣ����
      sw_sdcard_mode(); // SD��ģʽ
			switch (key)
						{
						case WKUP_PRES:                                                          
								frec_get_image_data(pixdatabuf, face_offx, face_offy, face_xsize, 30); // ��ȡͼ������
								res = atk_frec_recognition_face(pixdatabuf, &person);
								recognize_face_UI(person,res);
								NVIC_SystemReset();
								break;
						default:
								break;
						}
						sw_ov2640_mode(); // 2640ģʽ
						DCMI_Start();     // ��������
		}
		if(res==0)
		delay_ms(10);
		i++;
		if (i == 20) // DS0��˸.
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
    u8 selected_person = 0xFF; // ��ʼû��ѡ���κ���
    FRESULT fr1;
    FIL file1;
    UINT br1;
    char filename[30];
    char line[100];
    char name[20]={0};
    u8 name_count = 0;
    char name_list[MAX_FACES][20] = {0}; // �洢�����б�
    char person_ids[MAX_FACES] = {0};    // �洢��Ӧ��person ID
		OV2640_OutSize_Set(face_xsize, face_ysize);
		sw_sdcard_mode();                // SD��ģʽ
		res = atk_frec_initialization(); // ��ʼ������ʶ��
		exfuns_init();                    // Ϊfatfs��ر��������ڴ�
    f_mount(fs[0], "0:", 1);          // ����SD��
    f_mount(fs[1], "1:", 1);          // ����SPI FLASH
		if(f_mount(fs[0], "0:", 1) != FR_OK) {
				printf("SD������ʧ��!\r\n");
		} else {
				printf("SD�����سɹ�!\r\n");
		}
		//������ʾʶ����
		LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
		delay_ms(100);
    // �л���SD��ģʽ
		DCMI_Stop();
    sw_sdcard_mode();
    // ==================== SD����Ŀ¼��� ====================
    printf("\n=== ��ʼɾ������ ===\n");
    for(i=0;i<MAX_FACES;i++){
			// �����ļ���
        sprintf(filename, "0:/infor/person_%02d.txt", i);
				printf("���Դ��ļ�: %s\n", filename); // �������
				// ���ļ�
        fr1 = f_open(&file1, filename, FA_READ);
				if (fr1==FR_OK){
					printf("�ļ��򿪳ɹ�\n");
					// ��ȡ�ļ�����
					while (f_gets(line, sizeof(line), &file1)) {
							if (strstr(line, "Name:")) {
									sscanf(line, "Name: %19[^\r\n]", name);
									printf("�ҵ�����: %s\n", name);
									name_count++;
									strcpy((char*)name_list[name_count], name);
									person_ids[name_count] = i;
							} 
					}
				}
				else{
					printf("�ļ���ʧ��: %d\n", fr1);
				}
				f_close(&file1);
		}
    
    // ��������ʾ����
    LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
    LCD_ShowString(50, 20, 200, 24, 24, "DELETE USER");
    LCD_ShowString(50, 50, 200, 16, 16, "Select a user to delete:");
    
    // ���Ʒָ���
    LCD_DrawLine(50, 70, 430, 70);
		
    // ==================== �û�������ʾ ====================
    // ���û���ҵ��û�
    if(name_count == 0) {
        LCD_ShowString(50, 100, 200, 24, 24, "δ�ҵ��û�!");
        LCD_ShowString(50, 130, 200, 16, 16, "�������λ�÷���");
        
        while(1) {
            tp_dev.scan(0);
            if(tp_dev.sta & TP_PRES_DOWN) {
                return;
            }
            delay_ms(10);
        }
    }
    
    // ��ʾ�û��б�
    for(i = 0; i < name_count; i++) {
        // �����б����
        LCD_Fill(50, 80 + i*40, 430, 80 + (i+1)*40 - 2, WHITE);
        POINT_COLOR = BLACK;
        LCD_DrawRectangle(50, 80 + i*40, 430, 80 + (i+1)*40 - 2);
        
        // ��ʾ�û���
        LCD_ShowString(60, 85 + i*40, 200, 24, 24, name_list[i]);
        
        // ��ʾ�û�ID
        LCD_ShowString(350, 85 + i*40, 50, 24, 24, "ID:");
        LCD_ShowNum(380, 85 + i*40, person_ids[i], 2, 24);
    }
    
    // ���Ƶײ��ָ���
    POINT_COLOR = BLACK;
    LCD_DrawLine(50, 80 + name_count*40, 430, 80 + name_count*40);
    
    // ����ȷ�ϰ�ť
    POINT_COLOR = BLACK;
    LCD_DrawRectangle(150, 80 + name_count*40 + 10, 330, 80 + name_count*40 + 50);
    LCD_Fill(151, 80 + name_count*40 + 11, 329, 80 + name_count*40 + 49, RED);
    LCD_ShowString(210, 80 + name_count*40 + 20, 80, 24, 24, "DELETE");
    
    // ==================== ������ѭ�� ====================
    while(1) {
        tp_dev.scan(0);
        
        if(tp_dev.sta & TP_PRES_DOWN) {
            // ����Ƿ������û��б���
            for(i = 0; i < name_count; i++) {
                if(tp_dev.x[0] >= 50 && tp_dev.x[0] <= 430 && 
                   tp_dev.y[0] >= 80 + i*40 && tp_dev.y[0] <= 80 + (i+1)*40) {
                    // ȡ��֮ǰ��ѡ��
                    if(selected_person != 0xFF) {
                        j = selected_person;
                        LCD_Fill(50, 80 + j*40, 430, 80 + (j+1)*40 - 2, WHITE);
                        POINT_COLOR = BLACK;
                        LCD_DrawRectangle(50, 80 + j*40, 430, 80 + (j+1)*40 - 2);
                        LCD_ShowString(60, 85 + j*40, 200, 24, 24, name_list[j]);
                        LCD_ShowString(350, 85 + j*40, 50, 24, 24, "ID:");
                        LCD_ShowNum(380, 85 + j*40, person_ids[j], 2, 24);
                    }
                    
                    // �����µ�ѡ��
                    selected_person = i;
                    LCD_Fill(50, 80 + i*40, 430, 80 + (i+1)*40 - 2, BLUE);
                    POINT_COLOR = BLACK;
                    LCD_DrawRectangle(50, 80 + i*40, 430, 80 + (i+1)*40 - 2);
                    LCD_ShowString(60, 85 + i*40, 200, 24, 24, name_list[i]);
                    LCD_ShowString(350, 85 + i*40, 50, 24, 24, "ID:");
                    LCD_ShowNum(380, 85 + i*40, person_ids[i], 2, 24);
                    
                    // ��ʾ��ʾ��Ϣ
                    LCD_Fill(50, 80 + name_count*40 + 60, 430, 80 + name_count*40 + 100, WHITE);
                    LCD_ShowString(50, 80 + name_count*40 + 60, 200, 24, 24, "��ѡ��: ");
                    LCD_ShowString(150, 80 + name_count*40 + 60, 200, 24, 24, name_list[i]);
                    
                    break;
                }
            }
            
            // ����Ƿ�����ɾ����ť
            if(tp_dev.x[0] >= 150 && tp_dev.x[0] <= 330 && 
               tp_dev.y[0] >= 80 + name_count*40 + 10 && 
               tp_dev.y[0] <= 80 + name_count*40 + 50) {
                if(selected_person != 0xFF) {
                    // ��ť����Ч��
                    LCD_Fill(151, 80 + name_count*40 + 11, 329, 80 + name_count*40 + 49, BLUE);
                    LCD_ShowString(210, 80 + name_count*40 + 20, 80, 24, 24, "DELETE");
                    delay_ms(100);
                    LCD_Fill(151, 80 + name_count*40 + 11, 329, 80 + name_count*40 + 49, RED);
                    LCD_ShowString(210, 80 + name_count*40 + 20, 80, 24, 24, "DELETE");
                    
                    // �л���SD��ģʽ
                    sw_sdcard_mode();
                    
                    // ɾ������ģ��
                    printf("����ɾ������ģ�� %d...", person_ids[selected_person]);
                    res = atk_frec_delete_data(person_ids[selected_person]);
                    if(res == 0) {
                        printf("�ɹ�\n");
                        LCD_ShowString(50, 80 + name_count*40 + 90, 200, 24, 24, "ģ����ɾ��!");
                    } else {
                        printf("ʧ��! �������: %d\n", res);
                        LCD_ShowString(50, 80 + name_count*40 + 90, 200, 24, 24, "ģ��ɾ��ʧ��!");
                    }
                    
                    // ɾ���û���Ϣ�ļ�
                    sprintf(filename, "0:/infor/person_%02d.txt", person_ids[selected_person]);
                    printf("����ɾ���ļ� %s...", filename);
                    fr1 = f_unlink(filename);
                    if(fr1 == FR_OK) {
                        printf("�ɹ�\n");
                        LCD_ShowString(50, 80 + name_count*40 + 120, 200, 24, 24, "�ļ���ɾ��!");
                    } else {
                        printf("ʧ��! �������: %d\n", fr1);
                        LCD_ShowString(50, 80 + name_count*40 + 120, 200, 24, 24, "�ļ�ɾ��ʧ��!");
                    }
                    
                    // ���¼�������ʶ��ģ��
                    printf("���¼�������ģ��...");
                    atk_frec_load_data_model();
                    printf("���\n");
                    
                    // �л�������ͷģʽ
                    sw_ov2640_mode();
                    
                    // �ӳ���ʾ���
                    delay_ms(1500);
                    
                    // ����������
                    return;
                } else {
                    // û��ѡ���κ���
                    LCD_Fill(50, 80 + name_count*40 + 60, 430, 80 + name_count*40 + 100, WHITE);
                    LCD_ShowString(50, 80 + name_count*40 + 60, 200, 24, 24, "����ѡ���û�!");
                    delay_ms(1000);
                    LCD_Fill(50, 80 + name_count*40 + 60, 430, 80 + name_count*40 + 100, WHITE);
                }
            }
        }
        
        delay_ms(10);
    }

exit_fail:
    // ������
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
				if(tp_dev.sta&TP_PRES_DOWN)			//������������
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
				}else delay_ms(10);	//û�а������µ�ʱ�� 	
     
    }
}
