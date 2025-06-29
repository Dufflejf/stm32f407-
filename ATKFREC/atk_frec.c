#include "atk_frec.h"
#include "malloc.h"
#include "ff.h"
#include "stdio.h"

// 1. atk_frec_initialization() ��ʼ������ʶ��⣬�����ڲ��ṹ������Դ��
// 2. atk_frec_add_a_face()     ���һ������ģ�嵽�洢���ʣ��� SD ������
// 3. atk_frec_load_data_model() �������ѱ����ģ����ص��ڴ��У���ʶ���㷨ʹ�á�
// 4. atk_frec_recognition_face() ������ͼ��ִ��ʶ�𣬷�����ƥ���ģ��������
// 5. atk_frec_delete_data()    ɾ��ָ��������������ģ�塣
// 6. atk_frec_destroy()        ��������ʶ��⣬�ͷ������ڲ���Դ��


// �ڴ����ú����� SRAMEX ���ֽ����
void atk_frec_memset(char *p, char c, unsigned long len)
{
	mymemset((u8 *)p, (u8)c, (u32)len);
}

// �ڴ����뺯������ SRAMEX ��������ָ����С���ڴ�
void *atk_frec_malloc(unsigned int size)
{
	return mymalloc(SRAMEX, size);
}

// �ڴ��ͷź���
void atk_frec_free(void *ptr)
{
	myfree(SRAMEX, ptr);
}

// ��������ʶ�����������
// index:Ҫ���������λ��(һ����ռһ��λ��),��Χ:0~MAX_LEBEL_NUM-1
// buf:Ҫ��������ݻ������׵�ַ
// size:Ҫ��������ݴ�С
// ����ֵ:0,����
//     ����,�������
u8 atk_frec_save_data(u8 index, u8 *buf, u32 size)
{
	u8 *path; // ����ļ�·���ַ���
	FIL *fp; // FatFs �ļ�����
	u32 fw; // ʵ��д���ֽ���
	u8 res;
	path = atk_frec_malloc(30);		   // �����ڴ�
	fp = atk_frec_malloc(sizeof(FIL)); // ���� FIL �ṹ��
	if (!fp)
	{
		atk_frec_free(path);
		return ATK_FREC_MEMORY_ERR;
	}
	sprintf((char *)path, ATK_FREC_DATA_PNAME, index);
	f_mkdir(ATK_FREC_DATA_PDIR); // �����ļ���
	res = f_open(fp, (char *)path, FA_WRITE | FA_CREATE_NEW);
	if (res == FR_OK)
	{
		res = f_write(fp, buf, size, &fw); // д���ļ�
	}
	f_close(fp);
	if (res)
		res = ATK_FREC_READ_WRITE_ERR;
	atk_frec_free(path);
	atk_frec_free(fp);
	return res;
}
// ��ȡ����ʶ�����������
// index:Ҫ��ȡ������λ��(һ����ռһ��λ��),��Χ:0~MAX_LEBEL_NUM-1
// buf:Ҫ��ȡ�����ݻ������׵�ַ
// size:Ҫ��ȡ�����ݴ�С(size=0,���ʾ����Ҫ�����ݳ���)
// ����ֵ:0,����
//     ����,�������
u8 atk_frec_read_data(u8 index, u8 *buf, u32 size)
{
	u8 *path;
	FIL *fp;
	u32 fr;
	u8 res;
	path = atk_frec_malloc(30);		   // �����ڴ�
	fp = atk_frec_malloc(sizeof(FIL)); // �����ڴ�
	if (!fp)
	{
		atk_frec_free(path);
		return ATK_FREC_MEMORY_ERR;
	}
	sprintf((char *)path, ATK_FREC_DATA_PNAME, index);
	res = f_open(fp, (char *)path, FA_READ);
	if (res == FR_OK && size)
	{
		res = f_read(fp, buf, size, &fr); // ��ȡ�ļ�
		if (fr == size)
			res = 0;
		else
			res = ATK_FREC_READ_WRITE_ERR;
	}
	f_close(fp);
	if (res)
		res = ATK_FREC_READ_WRITE_ERR;
	atk_frec_free(path);
	atk_frec_free(fp);
	return res;
}
// ɾ��һ����������
// index:Ҫ���������λ��(һ����ռһ��λ��),��Χ:0~MAX_LEBEL_NUM-1
// ����ֵ:0,����
//     ����,�������
u8 atk_frec_delete_data(u8 index)
{
	u8 *path;
	u8 res;
	path = atk_frec_malloc(30); // �����ڴ�
	if (!path)
	{
		return ATK_FREC_MEMORY_ERR;
	}
	sprintf((char *)path, ATK_FREC_DATA_PNAME, index);
	res = f_unlink((char *)path);
	atk_frec_free(path);
	return res;
}
