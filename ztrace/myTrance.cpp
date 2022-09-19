// myTrance.cpp : 定义 DLL 应用程序的导出函数。
//

#include "myTrace.h"

#include <process.h>
#include <string>
#include <vector>
using namespace std;

/*****************************************************************************/
// utils

vector<string> get_args(char *params)
{
	vector<string> vec_param;

	char *p1, *p2;
	p1 = params;
	p2 = params;
	int ilen = strlen(params);
	while (p2[0]) {
		if (p2[0] == ' ') {
			p2[0] = 0;
			vec_param.push_back(p1);
			p2 += 1;
			p1 = p2;
		}
		p2++;
	}
	vec_param.push_back(p1);

	return vec_param;
}

unsigned __int64 py_int(const char *s, int unit)
{
	unsigned __int64 r = 0;
	if (unit == 10) {
		while (isdigit(s[0])) {
			r *= 10;
			r += ((unsigned int)(s[0] - '0'));
			s++;
		}
		return r;
	}
	if (unit == 16) {
		while (isxdigit(s[0])) {
			r *= 16;
			if (isdigit(s[0])) {
				r += ((unsigned int)(s[0] - '0'));
			}
			else if ('a' <= s[0] && s[0] <= 'f') {
				r += ((unsigned int)(s[0] - 'a' + 10));
			}
			else {
				r += ((unsigned int)(s[0] - 'A' + 10));
			}
			s++;
		}
		return r;
	}
	return 0;
}



void get_reg_diff(REGDUMP &last_reg, REGDUMP &this_reg, char *buff)
{
#define out_if_change(last_v, this_v, name)		\
if((last_v) != (this_v)) {						\
	sprintf(buff, "%s "##name##":%llX", buff, (this_v));	\
}
	// 	if (last_reg->regcontext.cax != this_reg->regcontext.cax) {
	// 		sprintf("%s rax:%X", buff, this_reg->regcontext.cax);
	// 	}
	out_if_change(last_reg.regcontext.cax, this_reg.regcontext.cax, "rax");
	out_if_change(last_reg.regcontext.cbx, this_reg.regcontext.cbx, "rbx");
	out_if_change(last_reg.regcontext.ccx, this_reg.regcontext.ccx, "rcx");
	out_if_change(last_reg.regcontext.cdx, this_reg.regcontext.cdx, "rdx");
	out_if_change(last_reg.regcontext.cbp, this_reg.regcontext.cbp, "rbp");
	out_if_change(last_reg.regcontext.csp, this_reg.regcontext.csp, "rsp");
	out_if_change(last_reg.regcontext.csi, this_reg.regcontext.csi, "rsi");
	out_if_change(last_reg.regcontext.cdi, this_reg.regcontext.cdi, "rdi");
#ifdef _WIN64
	out_if_change(last_reg.regcontext.r8, this_reg.regcontext.r8, "r8");
	out_if_change(last_reg.regcontext.r9, this_reg.regcontext.r9, "r9");
	out_if_change(last_reg.regcontext.r10, this_reg.regcontext.r10, "r10");
	out_if_change(last_reg.regcontext.r11, this_reg.regcontext.r11, "r11");
	out_if_change(last_reg.regcontext.r12, this_reg.regcontext.r12, "r12");
	out_if_change(last_reg.regcontext.r13, this_reg.regcontext.r13, "r13");
	out_if_change(last_reg.regcontext.r14, this_reg.regcontext.r14, "r14");
	out_if_change(last_reg.regcontext.r15, this_reg.regcontext.r15, "r15");
#endif
}

/*****************************************************************************/

void start_mytrace(vector<string> const & vec_param);

// 从菜单启动
void cb_menu_start_myTrace()
{
	char line[512];
	if (GuiGetLineWindow("input: start_ea end_ea", line)) {
		if (strlen(line) > 0) {
			vector<string> vec_param = get_args(line);
			start_mytrace(vec_param);
		}
	}
}

//经过调试观察, argc最大是2, 
//argv[0]就是原原本本的你在命令行输入的字符串: my_x64_plu2 E:\____wind\mypy\my_x64dbg_py_1.py -a1=aax -a2=bbb -a3=ee -a4="fff" -a5="hh gg"
//argv[1]则是去掉空格双引号之后的'缩进版': E:\____wind\mypy\my_x64dbg_py_1.pyaaxbbbcccddddeeeefffhh gg
// 从命令条启动
bool cb_cmd_start_myTrace(int argc, char *argv[])
{
	//myTrace 7FEe0e8aabb 7fee0f932a4
	//myTrace stop
	if (strstr(argv[0], "stop")) {
		stop_mytrace();
		return true;
	}

	char params[512];
	strcpy(params, argv[0] + strlen(PLUGIN_NAME) + 1);
	vector<string> vec_param = get_args(params);
	start_mytrace(vec_param);
	return true;
}






/*****************************************************************************/
using namespace Script;

class TraceData
{
public:
	void * m_end_ea;
	int m_maxcnt;

	int m_moduleCnt;
	Module::ModuleInfo m_modules[10];

	DWORD m_threadid;

	TraceData()
	{
		m_end_ea = NULL;
		m_maxcnt = 0;
		m_moduleCnt = 0;
	}

	void Load()
	{
#define EARANGE "EaRange"
#define MODULELIST "ModuleList"

		char dir[MAX_PATH];
		GetModuleFileNameA(NULL, dir, sizeof(dir));
		char * p = strrchr(dir, '\\');
		p[1] = 0;
		strcat(dir, "myTraceData.ini");

		char count[MAX_PATH];
		GetPrivateProfileStringA(EARANGE, "count", "0", count, sizeof(count), dir);
		m_maxcnt = py_int(count, 10);
		if (m_maxcnt == 0) {
			GetPrivateProfileStringA(EARANGE, "end_ea", "0", count, sizeof(count), dir);
			m_end_ea = (void *)py_int(count, 16);
		}

		for (int i = 0; i < 10; i += 1) {
			char module[64];
			sprintf(module, "module%d", i + 1);
			count[0] = 0;
			GetPrivateProfileStringA(MODULELIST, module, "", count, sizeof(count), dir);
			if (strlen(count) == 0) {
				break;
			}
			Module::ModuleInfo info;
			memset(&info, 0, sizeof(info));
			Module::InfoFromName(count, &info);
			if (info.base != 0) {
				m_modules[m_moduleCnt] = info;
				m_moduleCnt += 1;
			}
		}

#undef INIFILE
#undef EARANGE
#undef MODULELIST
	}

	bool isCipShouldRecord(duint cip)
	{
		for (int i = 0; i < m_moduleCnt; i++) {
			if (cip >= m_modules[i].base && cip < (m_modules[i].base + m_modules[i].size)) {
				return true;
			}
		}
		return false;
	}

	bool isEnoughStep(duint cip, int step)
	{
		if (m_maxcnt == 0) {
			return (m_end_ea == (void *)cip);
		}
		else {
			return step >= m_maxcnt;
		}
	}

	Module::ModuleInfo *get_moduleinfo(duint cip)
	{
		for (int i = 0; i < m_moduleCnt; i++) {
			if (cip >= m_modules[i].base && cip < (m_modules[i].base + m_modules[i].size)) {
				return &m_modules[i];
			}
		}
		return NULL;
	}
};


static bool thread_run = false;
uint32_t __stdcall trace_thread(void *p);

void start_mytrace(vector<string> const & vec_param)
{
	MessageBoxA(NULL, "hook chance", "myTrace", MB_OK);

	if (isTrue(DbgIsRunning())) {
		GuiDisplayWarning("myTrace", "paused dbg please");
		return;
	}

	// 停止之前的trace
	stop_mytrace();
	Sleep(1);

	REGDUMP regs1;
	DWORD threadid;
	get_paused_info(regs1, threadid);

	TraceData * p = new TraceData();
	p->Load();
	p->m_threadid = threadid;

	thread_run = true;
	_beginthreadex(NULL, 0, trace_thread, p, 0, NULL);

}

void stop_mytrace()
{
	thread_run = false;
}

uint32_t __stdcall trace_thread(void *_p)
{
	TraceData * tracedata = (TraceData *)_p;
	set_target_thread(tracedata->m_threadid);

	REGDUMP regs_old;
	memset(&regs_old, 0, sizeof(REGDUMP));

	FILE * fp = fopen("myTrace.txt", "wt");
	fputs("start trace\n", fp);
	dputs("start trace");

	GuiUpdateDisable();

	int step = 0;
	ULONG_PTR jmp_cip = 0;
	int calllevel = 0;
	duint callstack[20];
	while (true) {
		if (thread_run == false) {
			break;
		}

		REGDUMP regs_new;
		DWORD threadid;
		get_paused_info(regs_new, threadid);

		ULONG_PTR cip = regs_new.regcontext.cip;
		if (tracedata->isEnoughStep(cip, step)) {
			break;
		}

		if (jmp_cip != 0 && jmp_cip == cip) {
			fprintf(fp, ";--------------------------------------------------------------------------------\n");
		}
		if (calllevel > 0) {
			if (cip == callstack[calllevel - 1]) {
				calllevel--;
			}
		}
		


		char lable[MAX_LABEL_SIZE];
		lable[0] = 0;
		if (DbgGetLabelAt(cip, SEG_DEFAULT, lable)) {
			fprintf(fp, "%p %-19s", cip, lable);
		}
		else {
			Module::ModuleInfo *pinfo = tracedata->get_moduleinfo(cip);
			if (pinfo != NULL) {
				fprintf(fp, "%p %10s+%-8X", cip, pinfo->name, cip - pinfo->base);
			}
			else {
				fprintf(fp, "%p %-19s", cip, "-");
			}
		}

		DISASM_INSTR instr;
		DbgDisasmAt((duint)cip, &instr);

		char reg_diff[512];
		reg_diff[0] = 0;
		get_reg_diff(regs_old, regs_new, reg_diff);
		regs_old = regs_new;

		fprintf(fp, " ");
		for (int i = 0; i < calllevel; i++) {
			fprintf(fp, "|-> ");
		}
		fprintf(fp, "%-48s %s", instr.instruction, reg_diff);

		if (instr.argcount > 0) {
			if (instr.arg[0].type == DISASM_ARGTYPE::arg_memory) {
				duint value;
				DbgMemRead(instr.arg[0].value, &value, sizeof(duint));
				fprintf(fp, " [%llX]=%llX", instr.arg[0].value, value);
			}
			if (instr.argcount > 1) {
				if (instr.arg[1].type == DISASM_ARGTYPE::arg_memory) {
					duint value;
					DbgMemRead(instr.arg[1].value, &value, sizeof(duint));
					fprintf(fp, " [%llX]=%llX", instr.arg[1].value, value);
				}
			}
		}

		if (tracedata->isCipShouldRecord(cip) == false) {
			//现在执行到了<感兴趣的范围>之外

			//读取栈顶数据, 看看是否是个有效的'返回地址'
			auto rsp = regs_new.regcontext.csp;
			duint ret_addr;
			DbgMemRead((duint)rsp, &ret_addr, sizeof(duint));
			if (tracedata->isCipShouldRecord(ret_addr) == true) {
				fprintf(fp, " run_return_to %X", ret_addr);
				my_run_to(ret_addr);
			}
			else {
				thread_run = false;
			}
		}
		else {
			//判断下一条指令的去向
			if (instr.type == DISASM_INSTRTYPE::instr_branch) {
				jmp_cip = ((instr.arg[0].memvalue != 0) ? instr.arg[0].memvalue : instr.arg[0].value);
				if (tracedata->isCipShouldRecord(jmp_cip) == false) {
					lable[0] = 0;
					if (DbgGetLabelAt(jmp_cip, SEGMENTREG::SEG_DEFAULT, lable)) {
						fprintf(fp, " api[%s]", lable);
					}
					jmp_cip = 0;
					my_step_over();
				}
				else {
					if (strnicmp(instr.instruction, "call", 4) == 0) {
						duint ret_addr = (duint)cip + instr.instr_size;
						callstack[calllevel++] = ret_addr;
					}
					my_step_in();
				}
			}
			else {
				jmp_cip = 0;
				my_step_in();
			}
		}

		fprintf(fp, "\n");
// 		if (instr.type == DISASM_INSTRTYPE::instr_branch) {
// 			fprintf(fp, ";--------------------------------------------------------------------------------\n");
// 		}
		fflush(fp);
		step++;
	}
	thread_run = false;
	delete tracedata;

	GuiUpdateEnable(true);

	fputs("trace over\n", fp);
	fclose(fp);
	dputs("trace over");
	return 0;
}

