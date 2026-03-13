import subprocess
import sys
import os

def run_git(cmd):
    result = subprocess.run(['git'] + cmd, capture_output=True, text=True)
    return result

def main():
    if len(sys.argv) < 3:
        print("Usage: python fixers/prepare_pr.py <branch_name> \"<commit_message>\" [base_branch]")
        print('Example: python fixers/prepare_pr.py feature/clean-ui "Refactor UI and fix styles"')
        return

    branch_name = sys.argv[1]
    commit_message = sys.argv[2]
    # Try to find the best base branch
    base_branch = sys.argv[3] if len(sys.argv) > 3 else None
    
    if not base_branch:
        # Check standard remotes
        remotes = run_git(['remote']).stdout.split()
        if 'public-repo' in remotes:
            base_branch = 'public-repo/main'
        elif 'origin' in remotes:
            base_branch = 'origin/main'
        else:
            base_branch = 'main'

    print(f"--- Automated Clean PR Creation ---")
    print(f"Source: (Current State) -> Target Branch: {branch_name}")
    print(f"Base: {base_branch}")

    # 1. Stash any uncommitted work temporarily
    print("Saving current uncommitted work...")
    run_git(['add', '-A'])
    run_git(['stash'])

    # 2. Get current branch name to return later or squash from
    source_branch = run_git(['rev-parse', '--abbrev-ref', 'HEAD']).stdout.strip()

    # 3. Go to base branch and create NEW branch
    print(f"Creating fresh branch from {base_branch}...")
    run_git(['fetch', base_branch.split('/')[0] if '/' in base_branch else 'origin'])
    res = run_git(['checkout', '-b', branch_name, base_branch])
    
    if res.returncode != 0:
        print(f"Error: Could not create branch {branch_name}. Maybe it exists?")
        run_git(['stash', 'pop'])
        return

    # 4. Pull ALL changes from source branch as a single squash
    print(f"Squashing all work from {source_branch} into {branch_name}...")
    run_git(['merge', '--squash', source_branch])

    # 5. Re-apply any uncommitted work that was stashed
    print("Applying extra stashed changes...")
    run_git(['stash', 'pop'])
    
    # 6. Final Stage & Commit
    run_git(['add', '-A'])
    res = run_git(['commit', '-m', commit_message])

    if res.returncode == 0:
        print(f"\n✅ SUCCESS! Branch '{branch_name}' is ready with ONE clean commit.")
        print("-" * 40)
        print(f"To submit your PR, run:")
        remote_target = base_branch.split('/')[0] if '/' in base_branch else 'origin'
        print(f"git push {remote_target} {branch_name}")
    else:
        print("\n❌ FAILED: No changes detected to commit.")
        # Cleanup if failed
        run_git(['checkout', source_branch])
        run_git(['branch', '-D', branch_name])

if __name__ == "__main__":
    main()
