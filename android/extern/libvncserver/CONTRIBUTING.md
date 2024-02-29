# Pull Requests

* Make sure you only include relevant changes in your commit(s). In particular, don't re-format
  whole source files as those indentation changes add a lot of unrelated changes to your commit.

* Make your commits as [atomic as possible](https://www.freshconsulting.com/atomic-commits/).
   * Fundamental question 1: what could we need to revert later?
   * Fundamental question 2: what could we need to cherry-pick?
   * Fundamental question 3: is there an _and_ in the commit message? -> split it!

* Adhere to the commit message [guidelines](https://chris.beams.io/posts/git-commit/):
   * Start with the module you are changing, ended with a ':'. Common ones used here are "CMake:"
     or "examples:" or "libvncclient:", but there are more! A good way to find common module
     descriptions is to look into the git history of the project.
   * Keep the commit message short and in the form of "When applied, this commit will ' `<your commit message>`
   * Do _not_ end the subject line with a '.'.
   * Example: `warpdrive: increase fuel capacity to 100k`
