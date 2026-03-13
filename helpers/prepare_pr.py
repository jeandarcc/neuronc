import subprocess
import sys
import os

def run_git(cmd):
    result = subprocess.run(['git'] + cmd, capture_output=True, text=True)
    return result

def main():
    if len(sys.argv) < 3:
        print("Usage: python helpers/prepare_pr.py <branch_name> \"<message>\" [issue_id] [base_branch]")
        print('Example: python helpers/prepare_pr.py feature/rebranding "Project Rebranding" 1')
        return

    branch_name = sys.argv[1]
    raw_message = sys.argv[2]
    issue_id = sys.argv[3] if len(sys.argv) > 3 else None
    base_branch = sys.argv[4] if len(sys.argv) > 4 else None
    
    # Format according to your pipeline: [ISSUE#1] Project Rebranding
    if issue_id:
        clean_id = str(issue_id).strip().replace('#', '')
        commit_message = f"[ISSUE#{clean_id}] {raw_message}"
    else:
        commit_message = raw_message

    if not base_branch:
        remotes = run_git(['remote']).stdout.split()
        if 'public-repo' in remotes:
            base_branch = 'public-repo/main'
        elif 'origin' in remotes:
            base_branch = 'origin/main'
        else:
            base_branch = 'main'

    print(f"--- Automated Clean PR Creation ---")
    print(f"Branch: {branch_name}")
    print(f"Commit/PR Title: {commit_message}")

    # 1. Save state
    run_git(['add', '-A'])
    run_git(['stash'])

    source_branch = run_git(['rev-parse', '--abbrev-ref', 'HEAD']).stdout.strip()

    # 2. Fresh start from base
    run_git(['fetch', base_branch.split('/')[0] if '/' in base_branch else 'origin'])
    res = run_git(['checkout', '-b', branch_name, base_branch])
    
    if res.returncode != 0:
        print(f"Error: Could not create branch {branch_name}.")
        run_git(['stash', 'pop'])
        return

    # 3. Bring changes
    print(f"Merging changes from {source_branch} into one clean commit...")
    run_git(['merge', '--squash', source_branch])
    run_git(['stash', 'pop'])
    
    # 4. Final Commit (This becomes the PR default title)
    run_git(['add', '-A'])
    res = run_git(['commit', '-m', commit_message])

    if res.returncode == 0:
        print(f"\n✅ Ready! PR title will default to: {commit_message}")
        print("-" * 40)
        print(f"Run this to push: python helpers/push_pr.py {branch_name}")
    else:
        print("\n❌ Failed: No unique changes found.")
        run_git(['checkout', source_branch])

if __name__ == "__main__":
    main()
