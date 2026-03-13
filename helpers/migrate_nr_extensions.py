import os

def migrate_extensions(root_dir):
    count = 0
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith('.npp'):
                old_path = os.path.join(root, file)
                new_path = os.path.join(root, file[:-4] + '.nr')
                
                try:
                    if os.path.exists(new_path):
                        os.remove(new_path)
                    os.rename(old_path, new_path)
                    count += 1
                except Exception as e:
                    pass
    return count

def replace_in_files(root_dir):
    count = 0
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith(('.toml', '.nr', '.cpp', '.h', '.js', '.json', '.md')):
                path = os.path.join(root, file)
                try:
                    with open(path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    if '.npp' in content:
                        new_content = content.replace('.npp', '.nr')
                        with open(path, 'w', encoding='utf-8', newline='\n') as f:
                            f.write(new_content)
                        count += 1
                except:
                    pass
    return count

if __name__ == "__main__":
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    
    # 1. Physical Rename
    print("Step 1: Renaming .npp to .nr files...")
    rename_count = 0
    for item in os.listdir(project_root):
        item_path = os.path.join(project_root, item)
        if os.path.isdir(item_path) and not item.startswith('.'):
            rename_count += migrate_extensions(item_path)
    print(f"Total files renamed: {rename_count}")

    # 2. Text Replacement
    print("\nStep 2: Replacing '.npp' with '.nr' in source code...")
    text_count = replace_in_files(project_root)
    print(f"Total files updated: {text_count}")
