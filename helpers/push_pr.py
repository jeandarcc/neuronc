import subprocess
import sys

def run_git(cmd):
    result = subprocess.run(['git'] + cmd, capture_output=True, text=True)
    return result

def main():
    if len(sys.argv) < 2:
        print("Usage: python helpers/push_pr.py <remote_branch_name> [--force]")
        print("Example: python helpers/push_pr.py feature/rebranding")
        return

    remote_branch = sys.argv[1]
    force_flag = "--force" in sys.argv

    # Get current local branch name
    res = run_git(['rev-parse', '--abbrev-ref', 'HEAD'])
    if res.returncode != 0:
        print("Error: Could not determine current branch.")
        return
    
    local_branch = res.stdout.strip()
    
    print(f"Current branch: {local_branch}")
    print(f"Targeting: public-repo/{remote_branch}")

    # Construct push command
    push_cmd = ['push', 'public-repo', f"{local_branch}:{remote_branch}"]
    if force_flag:
        push_cmd.append('--force')

    print(f"Running: git {' '.join(push_cmd)}")
    
    # Execute push
    process = subprocess.Popen(['git'] + push_cmd, stdout=sys.stdout, stderr=sys.stderr)
    process.communicate()

    if process.returncode == 0:
        print("\n✅ Successfully pushed to public-repo!")
    else:
        print(f"\n❌ Push failed with exit code {process.returncode}")

if __name__ == "__main__":
    main()
