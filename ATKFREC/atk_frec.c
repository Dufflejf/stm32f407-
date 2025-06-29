#include "atk_frec.h"
#include "malloc.h"
#include "ff.h"
#include "stdio.h"

// 1. atk_frec_initialization() 初始化人脸识别库，分配内部结构所需资源。
// 2. atk_frec_add_a_face()     添加一张人脸模板到存储介质（如 SD 卡）。
// 3. atk_frec_load_data_model() 将所有已保存的模板加载到内存中，供识别算法使用。
// 4. atk_frec_recognition_face() 对输入图像执行识别，返回最匹配的模板索引。
// 5. atk_frec_delete_data()    删除指定索引处的人脸模板。
// 6. atk_frec_destroy()        销毁人脸识别库，释放所有内部资源。


// 内存设置函数， SRAMEX 逐字节填充
void atk_frec_memset(char *p, char c, unsigned long len)
{
	mymemset((u8 *)p, (u8)c, (u32)len);
}

// 内存申请函数，从 SRAMEX 区域申请指定大小的内存
void *atk_frec_malloc(unsigned int size)
{
	return mymalloc(SRAMEX, size);
}

// 内存释放函数
void atk_frec_free(void *ptr)
{
	myfree(SRAMEX, ptr);
}

// 保存人脸识别所需的数据
// index:要保存的数据位置(一张脸占一个位置),范围:0~MAX_LEBEL_NUM-1
// buf:要保存的数据缓存区首地址
// size:要保存的数据大小
// 返回值:0,正常
//     其他,错误代码
u8 atk_frec_save_data(u8 index, u8 *buf, u32 size)
{
	u8 *path; // 存放文件路径字符串
	FIL *fp; // FatFs 文件对象
	u32 fw; // 实际写入字节数
	u8 res;
	path = atk_frec_malloc(30);		   // 申请内存
	fp = atk_frec_malloc(sizeof(FIL)); // 申请 FIL 结构体
	if (!fp)
	{
		atk_frec_free(path);
		return ATK_FREC_MEMORY_ERR;
	}
	sprintf((char *)path, ATK_FREC_DATA_PNAME, index);
	f_mkdir(ATK_FREC_DATA_PDIR); // 创建文件夹
	res = f_open(fp, (char *)path, FA_WRITE | FA_CREATE_NEW);
	if (res == FR_OK)
	{
		res = f_write(fp, buf, size, &fw); // 写入文件
	}
	f_close(fp);
	if (res)
		res = ATK_FREC_READ_WRITE_ERR;
	atk_frec_free(path);
	atk_frec_free(fp);
	return res;
}
// 读取人脸识别所需的数据
// index:要读取的数据位置(一张脸占一个位置),范围:0~MAX_LEBEL_NUM-1
// buf:要读取的数据缓存区首地址
// size:要读取的数据大小(size=0,则表示不需要读数据出来)
// 返回值:0,正常
//     其他,错误代码
u8 atk_frec_read_data(u8 index, u8 *buf, u32 size)
{
	u8 *path;
	FIL *fp;
	u32 fr;
	u8 res;
	path = atk_frec_malloc(30);		   // 申请内存
	fp = atk_frec_malloc(sizeof(FIL)); // 申请内存
	if (!fp)
	{
		atk_frec_free(path);
		return ATK_FREC_MEMORY_ERR;
	}
	sprintf((char *)path, ATK_FREC_DATA_PNAME, index);
	res = f_open(fp, (char *)path, FA_READ);
	if (res == FR_OK && size)
	{
		res = f_read(fp, buf, size, &fr); // 读取文件
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
// 删除一个人脸数据
// index:要保存的数据位置(一张脸占一个位置),范围:0~MAX_LEBEL_NUM-1
// 返回值:0,正常
//     其他,错误代码
u8 atk_frec_delete_data(u8 index)
{
	u8 *path;
	u8 res;
	path = atk_frec_malloc(30); // 申请内存
	if (!path)
	{
		return ATK_FREC_MEMORY_ERR;
	}
	sprintf((char *)path, ATK_FREC_DATA_PNAME, index);
	res = f_unlink((char *)path);
	atk_frec_free(path);
	return res;
}
