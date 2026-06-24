// =============================================================================
// printer/mod.rs  —  Cross-platform receipt printing
//
// Windows (GDI Spooler or raw ESC/POS) and Android (network ESC/POS) are
// handled by separate platform modules selected via cfg attributes.
//
// The `PrintJob` struct and `print_receipt` function are the public contract
// shared by Tauri commands.  Platform modules implement `print_platform`.
// =============================================================================

use serde::Deserialize;

#[cfg(target_os = "windows")]
mod windows_gdi;
#[cfg(target_os = "android")]
mod android_net;

// ─────────────────────────────────────────────────────────────────────────────
// Shared types
// ─────────────────────────────────────────────────────────────────────────────

#[derive(Debug, Clone, Deserialize)]
pub struct LineItem {
    pub product_name: String,
    pub batch_number: String,
    pub expiry_date:  String,
    pub quantity:     i64,
    pub unit_price:   f64,
    pub line_total:   f64,
}

#[derive(Debug, Clone, Deserialize)]
pub struct PrintJob {
    pub pharmacy_name:    String,
    pub pharmacy_address: String,
    pub pharmacy_gstin:   String,
    pub pharmacy_dl:      String,
    pub bill_number:      String,
    pub bill_date:        String,
    pub customer_name:    Option<String>,
    pub payment_mode:     String,
    pub items:            Vec<LineItem>,
    pub taxable_amount:   f64,
    pub cgst_amount:      f64,
    pub sgst_amount:      f64,
    pub discount_amount:  f64,
    pub grand_total:      f64,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(tag = "mode")]
pub enum PrinterConfig {
    #[serde(rename = "gdi")]
    Gdi {
        printer_name:   String,
        paper_width_mm: u32,
    },
    #[serde(rename = "escpos_com")]
    EscPosCom {
        port:           String,
        baud_rate:      u32,
        paper_width_mm: u32,
        cut_paper:      bool,
    },
    #[serde(rename = "escpos_net")]
    EscPosNet {
        host:           String,
        port:           u16,
        paper_width_mm: u32,
        cut_paper:      bool,
    },
}

/// Print a receipt using the configured backend.
/// This is a synchronous function — call it from inside `spawn_blocking`.
pub fn print_receipt(job: &PrintJob, config: &PrinterConfig) -> Result<(), String> {
    #[cfg(target_os = "windows")]
    return windows_gdi::print(job, config);

    #[cfg(target_os = "android")]
    return android_net::print(job, config);

    #[cfg(not(any(target_os = "windows", target_os = "android")))]
    {
        log::warn!("Printing is not implemented on this platform");
        Ok(())
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared ESC/POS document builder (used by both COM and network backends)
// ─────────────────────────────────────────────────────────────────────────────

pub fn build_escpos(job: &PrintJob, col_width: usize, cut: bool) -> Vec<u8> {
    let mut b: Vec<u8> = Vec::with_capacity(2048);

    let pad_right = |s: &str, w: usize| -> String {
        if s.len() >= w { s[..w].to_string() } else { format!("{:<width$}", s, width = w) }
    };

    macro_rules! esc  { ($($x:expr),*) => { b.extend_from_slice(&[$($x),*]); }; }
    macro_rules! text { ($s:expr) => { b.extend_from_slice($s.as_bytes()); }; }
    macro_rules! lf   { ()        => { b.push(0x0A); }; }
    macro_rules! rule { ($w:expr) => {
        b.extend(std::iter::repeat(b'-').take($w));
        b.push(0x0A);
    }; }

    // ESC @ — init
    esc!(0x1B, 0x40);

    // Centre + bold + double-width for pharmacy name
    esc!(0x1B, 0x61, 0x01);   // centre
    esc!(0x1B, 0x45, 0x01);   // bold on
    esc!(0x1B, 0x21, 0x20);   // double width
    text!(job.pharmacy_name); lf!();
    esc!(0x1B, 0x21, 0x00);   // normal
    esc!(0x1B, 0x45, 0x00);   // bold off

    text!(&job.pharmacy_address); lf!();
    text!(&format!("GSTIN: {}", job.pharmacy_gstin)); lf!();
    text!(&format!("DL: {}",    job.pharmacy_dl));    lf!();
    rule!(col_width);

    esc!(0x1B, 0x61, 0x00);   // left align
    text!(&format!("Bill: {}", job.bill_number)); lf!();
    text!(&format!("Date: {}", job.bill_date));   lf!();
    if let Some(ref name) = job.customer_name {
        text!(&format!("Patient: {}", name)); lf!();
    }
    rule!(col_width);

    // Column header
    esc!(0x1B, 0x45, 0x01);
    text!(&format!("{:<20}{:>4}{:>8}{:>8}", "Medicine", "Qty", "Price", "Total")); lf!();
    esc!(0x1B, 0x45, 0x00);
    rule!(col_width);

    // Line items
    for item in &job.items {
        let name = pad_right(&item.product_name, 20);
        text!(&format!("{}{:>4}{:>8.2}{:>8.2}",
            name, item.quantity, item.unit_price, item.line_total));
        lf!();
        text!(&format!("  Batch:{:<10} Exp:{}", item.batch_number, item.expiry_date));
        lf!();
    }

    rule!(col_width);

    text!(&format!("{:<32}{:>8.2}", "Taxable:", job.taxable_amount)); lf!();
    text!(&format!("{:<32}{:>8.2}", "CGST:",    job.cgst_amount));    lf!();
    text!(&format!("{:<32}{:>8.2}", "SGST:",    job.cgst_amount));    lf!();
    if job.discount_amount > 0.0 {
        text!(&format!("{:<32}{:>8.2}", "Discount:", job.discount_amount)); lf!();
    }
    rule!(col_width);

    esc!(0x1B, 0x45, 0x01); esc!(0x1B, 0x21, 0x20);
    text!(&format!("TOTAL  INR {:.2}", job.grand_total)); lf!();
    esc!(0x1B, 0x21, 0x00); esc!(0x1B, 0x45, 0x00);
    rule!(col_width);

    text!(&format!("Payment: {}", job.payment_mode)); lf!();
    b.extend([0x0A, 0x0A, 0x0A]);

    esc!(0x1B, 0x61, 0x01);  // centre
    text!("Thank you! Get well soon."); lf!();
    b.extend([0x0A, 0x0A]);

    if cut {
        esc!(0x1D, 0x56, 0x41, 0x00);  // GS V A — full cut
    }
    b
}

// ─────────────────────────────────────────────────────────────────────────────
// Windows GDI implementation
// ─────────────────────────────────────────────────────────────────────────────

#[cfg(target_os = "windows")]
mod windows_gdi {
    use super::{PrintJob, PrinterConfig};
    use windows::Win32::Graphics::Gdi::*;
    use windows::Win32::Graphics::Printing::*;
    use windows::Win32::Foundation::BOOL;
    use windows::core::PCSTR;
    use std::ffi::CString;

    pub fn print(job: &PrintJob, config: &PrinterConfig) -> Result<(), String> {
        match config {
            PrinterConfig::Gdi { printer_name, .. } => {
                print_gdi(job, printer_name)
            }
            PrinterConfig::EscPosCom { port, baud_rate, paper_width_mm, cut_paper } => {
                print_escpos_com(job, port, *baud_rate, *paper_width_mm, *cut_paper)
            }
            _ => Err("This printer config is not supported on Windows".to_string()),
        }
    }

    fn print_gdi(job: &PrintJob, printer_name: &str) -> Result<(), String> {
        let pname = CString::new(printer_name).map_err(|e| e.to_string())?;
        let driver = CString::new("WINSPOOL").unwrap();

        // Safety: Win32 API call with valid C strings and null handles
        let hdc = unsafe {
            CreateDCA(PCSTR(driver.as_ptr() as *const u8),
                      PCSTR(pname.as_ptr()  as *const u8),
                      PCSTR::null(), None)
        };
        if hdc.is_invalid() {
            return Err(format!("CreateDC failed for '{}'", printer_name));
        }

        let doc_name = CString::new("Aushadhalayam Receipt").unwrap();
        let mut di   = DOCINFOA::default();
        di.cbSize      = std::mem::size_of::<DOCINFOA>() as i32;
        di.lpszDocName = PCSTR(doc_name.as_ptr() as *const u8);

        unsafe {
            if StartDocA(hdc, &di) <= 0 {
                DeleteDC(hdc);
                return Err("StartDoc failed".to_string());
            }
            StartPage(hdc);
            render_receipt_gdi(hdc, job);
            EndPage(hdc);
            EndDoc(hdc);
            DeleteDC(hdc);
        }
        Ok(())
    }

    unsafe fn render_receipt_gdi(hdc: HDC, job: &PrintJob) {
        let dpi_y  = GetDeviceCaps(hdc, LOGPIXELSY);
        let line_h = (dpi_y as f64 * 5.0 / 25.4) as i32;
        let mut y  = (dpi_y as f64 * 5.0 / 25.4) as i32;
        let x      = (dpi_y as f64 * 5.0 / 25.4) as i32;

        let font = CreateFontA(
            -MulDiv(9, dpi_y, 72), 0, 0, 0, FW_NORMAL.0 as i32,
            BOOL(0), BOOL(0), BOOL(0), ANSI_CHARSET.0 as u32,
            OUT_DEFAULT_PRECIS.0 as u32, CLIP_DEFAULT_PRECIS.0 as u32,
            DEFAULT_QUALITY.0 as u32, DEFAULT_PITCH.0 as u32 | FF_MODERN.0 as u32,
            PCSTR(b"Courier New\0".as_ptr()),
        );
        SelectObject(hdc, font);

        let mut text = |s: &str| {
            let cs = std::ffi::CString::new(s).unwrap_or_default();
            TextOutA(hdc, x, y, PCSTR(cs.as_ptr() as *const u8), s.len() as i32);
        };

        text(&job.pharmacy_name);   y += line_h;
        text(&job.pharmacy_address); y += line_h;
        text(&format!("Bill: {} Date: {}", job.bill_number, job.bill_date)); y += line_h;

        for item in &job.items {
            text(&format!("{:<22}{:>4}{:>8.2}", item.product_name, item.quantity, item.line_total));
            y += line_h;
        }

        text(&format!("Grand Total: INR {:.2}", job.grand_total));

        DeleteObject(font);
    }

    fn print_escpos_com(
        job:  &PrintJob,
        port: &str,
        baud: u32,
        w:    u32,
        cut:  bool,
    ) -> Result<(), String> {
        use windows::Win32::Storage::FileSystem::*;
        use windows::Win32::Foundation::*;
        use std::ffi::CString;

        let path = CString::new(format!("\\\\.\\{}", port)).map_err(|e| e.to_string())?;
        let handle = unsafe {
            CreateFileA(
                PCSTR(path.as_ptr() as *const u8),
                FILE_GENERIC_WRITE.0,
                FILE_SHARE_NONE,
                None,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                HANDLE::default(),
            )
        }
        .map_err(|e| format!("Cannot open {port}: {e}"))?;

        let doc = super::build_escpos(job, if w == 80 { 48 } else { 32 }, cut);
        let mut written = 0u32;
        unsafe {
            windows::Win32::Storage::FileSystem::WriteFile(
                handle,
                Some(&doc),
                Some(&mut written),
                None,
            )
        }
        .map_err(|e| format!("WriteFile failed: {e}"))?;

        unsafe { windows::Win32::Foundation::CloseHandle(handle).ok() };
        Ok(())
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Android ESC/POS over TCP (Wi-Fi thermal printers)
// ─────────────────────────────────────────────────────────────────────────────

#[cfg(target_os = "android")]
mod android_net {
    use super::{PrintJob, PrinterConfig};
    use std::io::Write;
    use std::net::TcpStream;
    use std::time::Duration;

    pub fn print(job: &PrintJob, config: &PrinterConfig) -> Result<(), String> {
        match config {
            PrinterConfig::EscPosNet { host, port, paper_width_mm, cut_paper } => {
                let addr = format!("{}:{}", host, port);
                let mut stream = TcpStream::connect(&addr)
                    .map_err(|e| format!("Cannot connect to printer at {addr}: {e}"))?;
                stream
                    .set_write_timeout(Some(Duration::from_secs(5)))
                    .ok();

                let col_w = if *paper_width_mm == 80 { 48 } else { 32 };
                let doc   = super::build_escpos(job, col_w, *cut_paper);
                stream.write_all(&doc).map_err(|e| e.to_string())?;
                Ok(())
            }
            _ => Err("Only escpos_net mode is supported on Android".to_string()),
        }
    }
}
