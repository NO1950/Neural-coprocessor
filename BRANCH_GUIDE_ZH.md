# 分支操作手册（PowerShell + GitHub）

> 两个目录，别搞混：
>
> | 目录 | 分支 | 用途 |
> |---|---|---|
> | `C:\Users\Administrator\Desktop\dlss5\10_upstream\nc-b3` | `field/fc6-20261022`（+ 未提交改动） | **日常开发/编译**，游戏里跑的就是这里编出来的 |
> | `C:\Users\Administrator\Desktop\dlss5\fork_cn` | `cn-all` | **发布用快照**，推到你 fork 的那条分支 |

---

## 一、PowerShell：看分支、切分支

```powershell
cd C:\Users\Administrator\Desktop\dlss5\fork_cn

git branch                 # 本地分支（当前那条前面有 *）
git branch -a              # 连远端分支一起列（remotes/origin/cn-all 这种）
git branch -vv             # 带上游跟踪关系
git status                 # 当前在哪条、有没有未提交改动
```

**切换分支**：

```powershell
git switch cn-all          # 切到 cn-all
git switch -c feature/xxx  # 新建并切过去
git switch -                # 切回上一条
```

> `git checkout <branch>` 也可以，但现在的写法是 `git switch`（切分支）/ `git restore`（还原文件）。

**拉取远端最新**：

```powershell
git fetch origin           # 我们 fork
git fetch upstream         # 作者仓库（fork_cn 里已加了这个 remote）
git log --oneline -3 upstream/main
git merge upstream/main    # 把作者最新合进当前分支（cn-all）
```

---

## 二、GitHub：看分支、改默认分支

### 看有哪些分支

- 仓库首页左上角 **分支下拉框**（写着 `main ▾` 那个）；
- 或直接开：`https://github.com/NO1950/Neural-coprocessor/branches`

### 把默认分支改成 `cn-all`

1. 打开 `https://github.com/NO1950/Neural-coprocessor/settings/branches`
2. **Default branch** → 点右侧的 ⇄ 图标
3. 下拉里选 **`cn-all`** → **Update**
4. 回首页，应该直接看到中文 README（`README_ZH.md` 会和作者的 `README.md` 并列显示）

> 改完以后，别人点进你仓库看到的就是中文说明 + 我们的版本，而不是 10-11 那条旧上传。

### 看某个文件在分支上的样子

- 首页分支下拉切到 `cn-all` → 点文件；
- 或在地址里写死：`https://github.com/NO1950/Neural-coprocessor/blob/cn-all/KEYS_AND_RISKS_ZH.md`

---

## 三、把日常改动同步到 `cn-all`（最常用）

日常改的是 `nc-b3`，要发布时才同步过去：

```powershell
# 1) 先确认日常树能编译
cd C:\Users\Administrator\Desktop\dlss5\10_upstream\nc-b3
build-b3.cmd

# 2) 同步到 fork_cn 并提交
python C:\Users\Administrator\Desktop\dlss5\70_tools\sync_cn_all.py

# 3) 看一眼 diff，没问题再推
cd C:\Users\Administrator\Desktop\dlss5\fork_cn
git log --oneline -1
git diff --stat HEAD~1
git push origin cn-all
```

`sync_cn_all.py` 做三件事：把 `src/`、`docs/`、`CMakeLists.txt`、`assets/panel_simple.txt`、两份中文文档复制过去，`git add -A`，然后**提交但不推送**（推不推由你决定）。

---

## 四、发 PR 的分支（和 cn-all 无关）

PR 必须从作者最新开、只含一个功能：

```powershell
cd C:\Users\Administrator\Desktop\dlss5
git clone https://github.com/maohgad-web/Neural-coprocessor.git pr_xxx
cd pr_xxx
git switch -c pr/xxx
git apply "…\补丁.patch"
git add -A
git commit -m "…"
git remote add fork https://github.com/NO1950/Neural-coprocessor.git
git push -u fork pr/xxx
```

**永远不要**用 `cn-all` 开 PR。

---

## 五、万一搞坏了怎么退

```powershell
git switch cn-all
git reset --hard origin/cn-all     # 本地丢弃所有改动，回到远端那条
```

或者只退一个文件：

```powershell
git restore --source=origin/cn-all -- src/panel_simple.cpp
```

（`reset --hard` 会丢掉未提交修改，执行前先 `git status` 看一眼。）
