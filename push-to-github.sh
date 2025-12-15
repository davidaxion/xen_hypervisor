#!/bin/bash
# Push to GitHub

set -e

echo "=== Pushing to GitHub ==="
echo "Repository: git@github.com:davidaxion/xen_hypervisor.git"
echo ""

# Check if remote exists
if git remote | grep -q origin; then
    echo "✓ Remote 'origin' already configured"
else
    echo "Adding remote..."
    git remote add origin git@github.com:davidaxion/xen_hypervisor.git
fi

# Show status
echo ""
echo "Current branch:"
git branch

echo ""
echo "Commits to push:"
git log --oneline origin/master..HEAD 2>/dev/null || git log --oneline -5

echo ""
echo "Pushing to GitHub..."
git push -u origin master

echo ""
echo "✓ Pushed successfully!"
echo ""
echo "View at: https://github.com/davidaxion/xen_hypervisor"
