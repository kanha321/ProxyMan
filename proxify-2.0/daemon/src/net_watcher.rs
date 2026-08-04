use common::LinkType;
use std::ptr::null_mut;
use windows_sys::Win32::NetworkManagement::IpHelper::{GetAdaptersAddresses, GAA_FLAG_INCLUDE_PREFIX, IP_ADAPTER_ADDRESSES_LH, IF_TYPE_IEEE80211, IF_TYPE_ETHERNET_CSMACD};
use windows_sys::Win32::NetworkManagement::Ndis::IfOperStatusUp;
use windows_sys::Win32::Networking::WinSock::AF_UNSPEC;

pub fn get_active_link_type() -> LinkType {
    unsafe {
        let mut out_buf_len: u32 = 15360;
        let mut buffer: Vec<u8> = vec![0; out_buf_len as usize];

        let mut dw_ret_val = GetAdaptersAddresses(
            AF_UNSPEC as u32,
            GAA_FLAG_INCLUDE_PREFIX,
            null_mut(),
            buffer.as_mut_ptr() as *mut IP_ADAPTER_ADDRESSES_LH,
            &mut out_buf_len,
        );

        if dw_ret_val == 111 { // ERROR_BUFFER_OVERFLOW
            buffer.resize(out_buf_len as usize, 0);
            dw_ret_val = GetAdaptersAddresses(
                AF_UNSPEC as u32,
                GAA_FLAG_INCLUDE_PREFIX,
                null_mut(),
                buffer.as_mut_ptr() as *mut IP_ADAPTER_ADDRESSES_LH,
                &mut out_buf_len,
            );
        }

        if dw_ret_val != 0 {
            return LinkType::Unknown;
        }

        let is_virtual = |desc: *const u16| -> bool {
            if desc.is_null() {
                return false;
            }
            let mut len = 0;
            while *desc.add(len) != 0 {
                len += 1;
            }
            let slice = std::slice::from_raw_parts(desc, len);
            let s = String::from_utf16_lossy(slice);
            let lower = s.to_lowercase();
            lower.contains("virtualbox")
                || lower.contains("vmware")
                || lower.contains("hyper-v")
                || lower.contains("virtual")
                || lower.contains("loopback")
                || lower.contains("bluetooth")
                || lower.contains("miniport")
                || lower.contains("tap")
        };

        let mut p_curr = buffer.as_ptr() as *const IP_ADAPTER_ADDRESSES_LH;

        // First pass: real physical Ethernet (wired) that is UP and has an IP
        while !p_curr.is_null() {
            let adapter = &*p_curr;
            if adapter.OperStatus == IfOperStatusUp
                && adapter.IfType == IF_TYPE_ETHERNET_CSMACD
                && !adapter.FirstUnicastAddress.is_null()
                && !is_virtual(adapter.Description)
            {
                return LinkType::Ethernet;
            }
            p_curr = adapter.Next;
        }

        // Second pass: Wi-Fi that is UP and has an IP
        p_curr = buffer.as_ptr() as *const IP_ADAPTER_ADDRESSES_LH;
        while !p_curr.is_null() {
            let adapter = &*p_curr;
            if adapter.OperStatus == IfOperStatusUp
                && adapter.IfType == IF_TYPE_IEEE80211
                && !adapter.FirstUnicastAddress.is_null()
            {
                return LinkType::WiFi;
            }
            p_curr = adapter.Next;
        }

        LinkType::Unknown
    }
}
