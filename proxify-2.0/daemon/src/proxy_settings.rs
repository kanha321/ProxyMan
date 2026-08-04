use windows_sys::Win32::System::Registry::{RegOpenKeyExW, RegSetValueExW, RegCloseKey, HKEY_CURRENT_USER, KEY_SET_VALUE, REG_DWORD, REG_SZ, HKEY};
use windows_sys::Win32::Networking::WinInet::{InternetSetOptionW, INTERNET_OPTION_SETTINGS_CHANGED, INTERNET_OPTION_REFRESH};
use std::ptr::null;

const INTERNET_SETTINGS_KEY: &str = r"Software\Microsoft\Windows\CurrentVersion\Internet Settings";

fn to_wstring(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(std::iter::once(0)).collect()
}

pub fn set_system_proxy(proxy_host_port: &str) -> bool {
    unsafe {
        let key_path = to_wstring(INTERNET_SETTINGS_KEY);
        let mut h_key: HKEY = std::ptr::null_mut();
        if RegOpenKeyExW(HKEY_CURRENT_USER, key_path.as_ptr(), 0, KEY_SET_VALUE, &mut h_key) != 0 {
            eprintln!("[ProxySettings] Failed to open registry key");
            return false;
        }

        let dw_enable: u32 = 1;
        let val_enable = to_wstring("ProxyEnable");
        RegSetValueExW(
            h_key,
            val_enable.as_ptr(),
            0,
            REG_DWORD,
            &dw_enable as *const u32 as *const u8,
            std::mem::size_of::<u32>() as u32,
        );

        let val_server = to_wstring("ProxyServer");
        let w_proxy = to_wstring(proxy_host_port);
        RegSetValueExW(
            h_key,
            val_server.as_ptr(),
            0,
            REG_SZ,
            w_proxy.as_ptr() as *const u8,
            (w_proxy.len() * 2) as u32,
        );

        let val_override = to_wstring("ProxyOverride");
        let w_override = to_wstring("localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*;<local>");
        RegSetValueExW(
            h_key,
            val_override.as_ptr(),
            0,
            REG_SZ,
            w_override.as_ptr() as *const u8,
            (w_override.len() * 2) as u32,
        );

        RegCloseKey(h_key);

        InternetSetOptionW(null(), INTERNET_OPTION_SETTINGS_CHANGED, null(), 0);
        InternetSetOptionW(null(), INTERNET_OPTION_REFRESH, null(), 0);

        println!("[ProxySettings] System proxy ENABLED -> {}", proxy_host_port);
        true
    }
}

pub fn clear_system_proxy() -> bool {
    unsafe {
        let key_path = to_wstring(INTERNET_SETTINGS_KEY);
        let mut h_key: HKEY = std::ptr::null_mut();
        if RegOpenKeyExW(HKEY_CURRENT_USER, key_path.as_ptr(), 0, KEY_SET_VALUE, &mut h_key) != 0 {
            eprintln!("[ProxySettings] Failed to open registry key");
            return false;
        }

        let dw_enable: u32 = 0;
        let val_enable = to_wstring("ProxyEnable");
        RegSetValueExW(
            h_key,
            val_enable.as_ptr(),
            0,
            REG_DWORD,
            &dw_enable as *const u32 as *const u8,
            std::mem::size_of::<u32>() as u32,
        );

        RegCloseKey(h_key);

        InternetSetOptionW(null(), INTERNET_OPTION_SETTINGS_CHANGED, null(), 0);
        InternetSetOptionW(null(), INTERNET_OPTION_REFRESH, null(), 0);

        println!("[ProxySettings] System proxy DISABLED");
        true
    }
}
