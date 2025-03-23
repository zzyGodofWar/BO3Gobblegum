#include <Windows.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <TlHelp32.h>

#include <map>

const unsigned offset_reflex = 0x380;
const unsigned offset_bubblegum = 0x999;

struct GobblegumMemoryInfo{
    char name[64];
    void* zmui_bgb_address;
    void* zm_bgb_address;
    void* t7_hud_zm_bgb_address;
    void* zmui_bgb_desc_address;
    char zmui_bgb_origin[64] ;
    char zm_bgb_origin[64] ;
    char t7_hud_zm_bgb_origin[64] ;
    char zmui_bgb_desc_origin[64];
};

std::map<std::string, GobblegumMemoryInfo> classic_gobblegum_map;
std::map<std::string, GobblegumMemoryInfo> mega_gobblegum_map;
std::map<std::string, GobblegumMemoryInfo> gobblegum_map;

const char *gobbleGum_classic[] = {
    "always_done_swiftly", "arms_grace", "coagulant", 
    "stock_option", "sword_flay", "armamental_accomplishment",
    "firing_on_all_cylinders", "arsenal_accelerator", "lucky_crit", 
    "now_you_see_me", "alchemical_antithesis", "tone_death", 
    "in_plain_sight", "newtonian_negation", "projectile_vomiting",
    //"anywhere_but_here", 
    
    //"board_games", "extra_credit",
    //"board_to_death", "flavor_hexed"

};

//Impatient
//tone_death
//reign_drops
//Board To Death
//Board Games


//' "eye_candy", "newtonian_negation","projectile_vomiting", "tone_death"

const char* gobbleGum_mega[] = { 
    "shopping_free", "perkaholic", "kill_joy", 
    "cache_back", "wall_power", "near_death_experience", 
    "immolation_liquidation", "undead_man_walking", "phoenix_up",
    "secret_shopper", "crawl_space", "idle_eyes", 
    "crate_power", "round_robbin", "profit_sharing",
    //"killing_time", 
    //"reign_drops", "reign_drops",
    //"power_vacuum", "power_vacuum"
};



uintptr_t FindStringTableAddress(HANDLE hProcess, uintptr_t startAddress)
{
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t address = startAddress;

    while (VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        /*std::wcout << L"Base Address: 0x" << std::hex << mbi.BaseAddress
            << L" | Size: 0x" << mbi.RegionSize
            << L" | State: " << mbi.State
            << L" | Protect: " << mbi.Protect
            << L" | Type: " << mbi.Type
            << std::endl;*/

        if (mbi.RegionSize == 0x29280000 && mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE) {
            char buffer[16] = {};
            SIZE_T read;

            std::wcout << L"Maybe String Page Address: 0x" << std::hex << address;

            for (SIZE_T block = 0; block < mbi.RegionSize; block += 0x1000) {
                address = (uintptr_t)((char*)mbi.BaseAddress + block);

                if (ReadProcessMemory(hProcess, (char*)address + offset_reflex, buffer, 6, &read)
                    && !strncmp("reflex", buffer, 6)) {                

                    if (ReadProcessMemory(hProcess, (char*)address + offset_bubblegum, buffer, 10, &read)
                        && !strcmp("bubblegum", buffer)) {

                        //std::wcout << L"String Page Address: 0x" << std::hex << address;

                        return address;
                        break;
                    }
                }

                
            }

            break;
        }


        address = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }

    return 0;
}


DWORD GetProcessIdByNameW(const wchar_t *name) {
    HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    PROCESSENTRY32 tProcess = { 0 };
    tProcess.dwSize = sizeof(tProcess);

    if (Process32First(handle, &tProcess)) {
        do {
            if (!_wcsicmp(tProcess.szExeFile, name)) {
                CloseHandle(handle);
                return tProcess.th32ProcessID;
            }
        } while (Process32Next(handle, &tProcess));
    }
    CloseHandle(handle);
    return 0;
}



void SuspendProcess(HANDLE processHandle) {
    typedef LONG(NTAPI* NtSuspendProcess)(IN HANDLE ProcessHandle);
    static NtSuspendProcess pfnNtSuspendProcess =
        (NtSuspendProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtSuspendProcess");
    pfnNtSuspendProcess(processHandle);
}


void ResumeProcess(HANDLE processHandle) {
    typedef LONG(NTAPI* NtResumeProcess)(IN HANDLE ProcessHandle);
    static NtResumeProcess pfnNtResumeProcess =
        (NtResumeProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtResumeProcess");
    pfnNtResumeProcess(processHandle);
}

void ModifyGobblegum(HANDLE processHandle, GobblegumMemoryInfo& src, const GobblegumMemoryInfo& target) {
    SIZE_T written;
    WriteProcessMemory(processHandle, src.zmui_bgb_address, target.zmui_bgb_origin, strlen(target.zmui_bgb_origin) + 1, &written);
    if (src.zm_bgb_address) {
        if (target.zm_bgb_address) {
            WriteProcessMemory(processHandle, src.zm_bgb_address, target.zm_bgb_origin, strlen(target.zm_bgb_origin) + 1, &written);
        }
        else {
            std::string temp = "zm_bgb_";
            temp.append(target.name);
            WriteProcessMemory(processHandle, src.zm_bgb_address, temp.c_str(), temp.length() + 1, &written);
        }        
    }
    WriteProcessMemory(processHandle, src.t7_hud_zm_bgb_address, target.t7_hud_zm_bgb_origin, strlen(target.t7_hud_zm_bgb_origin) + 1, &written);
    WriteProcessMemory(processHandle, src.zmui_bgb_desc_address, target.zmui_bgb_desc_origin, strlen(target.zmui_bgb_desc_origin) + 1, &written);
}

void RestoreGobblegum(HANDLE processHandle, GobblegumMemoryInfo & src) {
    SIZE_T written;
    WriteProcessMemory(processHandle, src.zmui_bgb_address, src.zmui_bgb_origin, strlen(src.zmui_bgb_origin) + 1, &written);
    if (src.zm_bgb_address) {
        WriteProcessMemory(processHandle, src.zm_bgb_address, src.zm_bgb_origin, strlen(src.zm_bgb_origin) + 1, &written);
    }
    WriteProcessMemory(processHandle, src.t7_hud_zm_bgb_address, src.t7_hud_zm_bgb_origin, strlen(src.t7_hud_zm_bgb_origin) + 1, &written);
    WriteProcessMemory(processHandle, src.zmui_bgb_desc_address, src.zmui_bgb_desc_origin, strlen(src.zmui_bgb_desc_origin) + 1, &written);
}


DWORD64 GetProcessModuleHandleW(DWORD PID, LPWCH ModuleName) {
    HANDLE hSnapShot;
    MODULEENTRY32W buffer;
    PROCESSENTRY32W probuffer;
    if (PID == 0) {
        return 0;
    }

    if (!ModuleName) {
        hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        probuffer.dwSize = sizeof(PROCESSENTRY32);
        if (hSnapShot == INVALID_HANDLE_VALUE) {
            return 0;
        }
        if (Process32First(hSnapShot, &probuffer)) {
            do {
                if (probuffer.th32ProcessID == PID) {
                    break;
                }
            } while (Process32Next(hSnapShot, &probuffer));
        }
        else {
            CloseHandle(hSnapShot);
            return 0;
        }
        CloseHandle(hSnapShot);
    }
    else {
        wcscpy_s(probuffer.szExeFile, ModuleName);
    }

    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, PID);

    buffer.dwSize = sizeof(MODULEENTRY32W);

    if (Module32First(hSnapShot, &buffer)) {
        do {
            if (!_wcsicmp(buffer.szModule, probuffer.szExeFile)) {
                CloseHandle(hSnapShot);
                return *(DWORD64*)&buffer.modBaseAddr;
            }
        } while (Module32Next(hSnapShot, &buffer));
    }
    CloseHandle(hSnapShot);
    return 0;
}


int main()
{
    DWORD pid = GetProcessIdByNameW(L"BlackOps3.exe");

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hProcess == NULL)
    {
        std::cerr << "Failed to open process. Error code: " << GetLastError() << std::endl;
        return 1;
    }

    DWORD64 tableAddress = FindStringTableAddress(hProcess, 0);
    if (!tableAddress) {
        std::cerr << "Failed to query string table."  << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    GobblegumMemoryInfo temp;
    std::string currentName;
    auto pstring = (tableAddress + offset_bubblegum);
    while(true){
        char buffer[64] = {};
        SIZE_T read;

        if (ReadProcessMemory(hProcess, (char*)pstring, buffer, sizeof(buffer), &read)) {
            if (buffer[1] == 0x5) {
                break;
            
            }

            std::string text = std::string(buffer);

            if (!text.find("zmui_bgb_")) {
                if (text.rfind("_desc") == text.size() - 5) {
                    temp.zmui_bgb_desc_address = (void*)pstring;
                    strcpy_s(temp.zmui_bgb_desc_origin, buffer);

                    if (temp.zm_bgb_address) {
                        classic_gobblegum_map.insert(std::make_pair<>(currentName, temp));
                    }
                    else {
                        mega_gobblegum_map.insert(std::make_pair<>(currentName, temp));
                    }
                    gobblegum_map.insert(std::make_pair<>(currentName, temp));
                }
                else {
                    memset(&temp, 0, sizeof(temp));
                    temp.zmui_bgb_address = (void*)pstring;
                    strcpy_s(temp.zmui_bgb_origin, buffer);

                    currentName = text.substr(9);
                    strcpy_s(temp.name, currentName.c_str());

                }
            }else if (text.find("zm_bgb_") == 0) {
                temp.zm_bgb_address = (void*)pstring;
                strcpy_s(temp.zm_bgb_origin, buffer);

            }else if (text.find("t7_hud_zm_bgb_") == 0) {
                temp.t7_hud_zm_bgb_address = (void*)pstring;
                strcpy_s(temp.t7_hud_zm_bgb_origin, buffer);

            }

            std::cout << (void*)pstring << "\t" << buffer << "\n";
            pstring += strlen(buffer) + 1;
        }
        else {
            break;
        }
        
    }

    std::cout << "按回车键继续...\n";
    char ch = getchar();

    if (ch != 'r') {
        for (int i = 0; i < sizeof(gobbleGum_classic) / sizeof(gobbleGum_classic[0]); i++) {
            ModifyGobblegum(hProcess,
                gobblegum_map[gobbleGum_classic[i]],
                gobblegum_map[gobbleGum_mega[i]]);
        }

        CloseHandle(hProcess);
        return 0;
    }

    std::cout << "round_robbin\n";


    ModifyGobblegum(hProcess,
        classic_gobblegum_map["always_done_swiftly"],
           mega_gobblegum_map["round_robbin"]);

    ModifyGobblegum(hProcess,
        classic_gobblegum_map["in_plain_sight"],
           mega_gobblegum_map["round_robbin"]);

    ModifyGobblegum(hProcess,
        classic_gobblegum_map["stock_option"],
           mega_gobblegum_map["round_robbin"]);

    ModifyGobblegum(hProcess,
        classic_gobblegum_map["arms_grace"],
           mega_gobblegum_map["perkaholic"]);

    ModifyGobblegum(hProcess,
        classic_gobblegum_map["newtonian_negation"],
        mega_gobblegum_map["round_robbin"]);

    ModifyGobblegum(hProcess,
        classic_gobblegum_map["projectile_vomiting"],
        mega_gobblegum_map["round_robbin"]);


    CloseHandle(hProcess);

    
    return 0;
}
