import subprocess
import os

def run_git(cmd):
    result = subprocess.run(['git'] + cmd, capture_output=True, text=True)
    return result

def resolve_templates_from_public():
    """
    Forcefully updates .github templates from public-repo/main
    """
    print("Syncing GitHub templates from public-repo/main...")
    
    # 1. Update info from remote
    run_git(['fetch', 'public-repo'])
    
    # 2. Checkout specifically the templates directory from public-repo
    # This acts like a 'theirs' resolution for that specific folder
    target = ".github/ISSUE_TEMPLATE/"
    res = run_git(['checkout', 'public-repo/main', '--', target])
    
    if res.returncode == 0:
        print(f"Successfully synced {target}")
        run_git(['add', target])
        print("Changes added to index. You can now commit.")
    else:
        print(f"Failed to sync templates: {res.stderr}")

if __name__ == "__main__":
    # This script assumes 'public-repo' remote exists
    resolve_templates_from_public()
