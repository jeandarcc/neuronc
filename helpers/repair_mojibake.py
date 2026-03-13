import os

# Special mapping for CP1252 "glitches" that occur when UTF-8 bytes 
# are incorrectly interpreted as Windows-1252/Latin-1 characters.
CP1252_MAP = {
    0x20AC: 0x80, 0x201A: 0x82, 0x0192: 0x83, 0x201E: 0x84, 0x2026: 0x85,
    0x2020: 0x86, 0x2021: 0x87, 0x02C6: 0x88, 0x2030: 0x89, 0x0160: 0x8A,
    0x2039: 0x8B, 0x0152: 0x8C, 0x017D: 0x8E, 0x2018: 0x91, 0x2019: 0x92,
    0x201C: 0x93, 0x201D: 0x94, 0x2022: 0x95, 0x2013: 0x96, 0x2014: 0x97,
    0x02DC: 0x98, 0x2122: 0x99, 0x0161: 0x9A, 0x203A: 0x9B, 0x0153: 0x9C,
    0x017E: 0x9E, 0x0178: 0x9F
}

def recover_bytes(text):
    """
    Backtracks a mojibake string by converting CP1252 characters back 
    to their raw byte values.
    """
    out = bytearray()
    for char in text:
        cp = ord(char)
        if cp in CP1252_MAP:
            out.append(CP1252_MAP[cp])
        elif cp < 256:
            out.append(cp)
        else:
            # Fallback for characters outside Latin-1/CP1252 range
            out.extend(char.encode('utf-8')) 
    return out

def fix_mojibake(file_path):
    """
    Detects and repairs double-encoded UTF-8 files.
    """
    try:
        if not os.path.isfile(file_path):
            return False
            
        with open(file_path, 'rb') as f:
            raw_bytes = f.read()
        
        if not raw_bytes: 
            return False
        
        # Decode as UTF-8 (stripping BOM if present)
        try:
            content = raw_bytes.decode('utf-8-sig')
        except UnicodeDecodeError:
            return False
        
        # Try to recover original bytes
        recovered_bytes = recover_bytes(content)
        
        try:
            # If the recovered bytes form a valid UTF-8 string that is different 
            # from current content, we have successfully fixed a mojibake.
            fixed_content = recovered_bytes.decode('utf-8')
            if fixed_content != content:
                # Save as clean UTF-8 without BOM
                with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
                    f.write(fixed_content)
                return True
        except UnicodeDecodeError:
            pass
            
    except Exception as e:
        print(f"Error processing {file_path}: {e}")
    return False

def run_fixer(target_dir):
    print(f"Scanning for encoding issues in: {target_dir}")
    fixed_count = 0
    for root, dirs, files in os.walk(target_dir):
        # Skip git and backups to be safe
        if '.git' in root or 'backups' in root:
            continue
            
        for file in files:
            if file.endswith(('.toml', '.nr', '.cpp', '.h', '.c', '.md')):
                path = os.path.join(root, file)
                if fix_mojibake(path):
                    print(f"Fixed encoding: {path}")
                    fixed_count += 1
    
    print(f"\nFinal report: {fixed_count} files repaired.")

if __name__ == "__main__":
    # Target the diagnostics folder by default, but can be scaled to root
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    diag_dir = os.path.join(project_root, "config", "diagnostics")
    
    if os.path.exists(diag_dir):
        run_fixer(diag_dir)
    else:
        print(f"Directory not found: {diag_dir}")
