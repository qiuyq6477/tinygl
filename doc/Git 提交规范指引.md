
一、 核心提交格式
每个提交信息必须包含 Header，可选包含 Body 和 Footer。各部分之间需用空行隔开。

```cpp
<type>(<scope>): <subject>

[optional body]

[optional footer(s)]

```

二、 Header 规范（严格遵守）
Header 是提交信息的首行，字数建议控制在 50个字符以内。

1. 提交类型 (Type)
必须使用以下预定义前缀之一：
feat     新功能 (Feature)
fix         修复 Bug
docs     文档更新 (README, API 等)
style     格式调整（不影响逻辑，如空格、分号等）
refactor 代码重构（既非修复也非新功能）
perf     性能优化
test     新增或修改测试用例
build     影响构建系统或外部依赖（如 npm, maven, docker 等）
ci         持续集成配置变更 (GitHub Actions, Jenkins 等)
chore     杂项工作（不修改源代码或测试文件）
revert     撤销之前的某次提交

2. 作用域 (Scope) - 可选
用括号包裹，说明此次改动影响的具体模块（如 (auth), (ui), (parser)）。
3. 主题描述 (Subject)
动词开头：使用祈使句、一般现在时（例如用 add 而非 added 或 adds）。
不加标点：结尾不带句号。
全小写：建议主体部分首字母小写（除非是专有名词）。

三、 Body & Footer 规范

1. 正文 (Body) - 推荐
解释“为什么”要做此改动，而非“怎么做”（代码本身已说明如何做）。
每行建议不超过 72个字符，以保持在终端中的可读性。
2. 脚注 (Footer) - 可选
破坏性变更 (Breaking Change)：必须以 BREAKING CHANGE: 开头，后跟空格和详细说明，或在 type 后加 !（如 feat!: ...）。
引用 Issue：关闭或引用问题单，例如 Fixes #123 或 Closes #456。

四、 黄金法则 (Best Practices)
原子提交：一次提交只做一件事。如果一个功能既涉及重构又涉及新功能，请分两次提交。
严禁模糊：杜绝使用 update, fix, more work 等没有任何实质意义的描述。
重大变更必标：任何会导致 API 不兼容或系统行为重大变化的改动，必须在 Footer 明确标出 BREAKING CHANGE。
先测试后提交：确保提交的代码能够通过本地测试和 CI 检查。
