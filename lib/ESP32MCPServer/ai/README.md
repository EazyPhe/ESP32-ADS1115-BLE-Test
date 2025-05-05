# AI Development Tools

This directory contains tools and scripts for AI-assisted development of the ESP32 MCP Server project.

## Directory Structure

```
ai/
├── README.md             # This file
├── scripts/             # Python scripts for AI automation
│   ├── __init__.py
│   └── ai_dev_helper.py  # Main AI development helper script
├── templates/           # Templates for AI-generated content
│   ├── docs/
│   ├── tests/
│   └── pr/
└── ai-dev              # Shell script wrapper for easy usage
```

## Setup

1. Ensure Python 3.7+ is installed
2. Install dependencies:
   ```bash
   pip install -r ai/requirements.txt
   ```
3. Make the wrapper script executable:
   ```bash
   chmod +x ai/ai-dev
   ```
4. (Optional) Add to your PATH:
   ```bash
   export PATH=$PATH:/path/to/project/ai
   ```

## Usage

### Create New Feature
```bash
ai-dev feature add-metrics-visualization
```
This will:
- Create a feature branch
- Setup documentation structure
- Generate test templates
- Prepare development notes

### Generate PR Description
```bash
ai-dev pr feature/my-feature
```
Generates a PR description including:
- Feature overview
- Changes list
- AI usage documentation
- Testing checklist

### Generate Tests
```bash
ai-dev test MetricsSystem
```
Creates:
- Test file structure
- Basic test cases
- Setup/teardown methods

### Update Documentation
```bash
ai-dev docs NetworkManager
```
Generates/updates:
- API documentation
- Usage examples
- Integration guides

### Review Code
```bash
ai-dev review --pattern "src/*.cpp"
```
Performs:
- Code review
- Best practices check
- Improvement suggestions

## Configuration

The tools use `.aidev.json` for configuration:

```json
{
  "prompts": {
    "pr": "PR description template",
    "test": "Test generation template",
    "docs": "Documentation template"
  },
  "history": [
    {
      "timestamp": "ISO-8601",
      "tool": "tool-name",
      "prompt": "prompt-used",
      "output": "ai-output"
    }
  ],
  "templates": {
    "pr": "PR template",
    "feature": "Feature template"
  }
}
```

## Best Practices

1. **AI Review**
   - Always review AI-generated code
   - Test thoroughly
   - Document AI usage

2. **Iterative Development**
   - Use AI for initial implementation
   - Review and refine
   - Document changes

3. **Documentation**
   - Keep AI prompts in version control
   - Document successful patterns
   - Share learning with team

4. **Security**
   - Never share sensitive data with AI
   - Review security implications
   - Validate cryptographic code

## Examples

### Feature Development
```bash
# Create new feature
ai-dev feature add-metrics-export 

# Generate tests
ai-dev test MetricsExporter

# Update documentation
ai-dev docs MetricsExporter

# Review changes
ai-dev review --pattern "src/metrics_exporter.*"

# Generate PR
ai-dev pr feature/metrics-export
```

### Documentation Update
```bash
# Update component docs
ai-dev docs NetworkManager

# Review changes
ai-dev review --pattern "docs/*.md"
```

### Test Development
```bash
# Generate test structure
ai-dev test NewComponent

# Run tests
pio test -e native
```

## Troubleshooting

### Common Issues

1. **Script not found**
   ```bash
   export PATH=$PATH:/path/to/project/ai
   ```

2. **Python dependencies**
   ```bash
   pip install -r ai/requirements.txt
   ```

3. **Permission denied**
   ```bash
   chmod +x ai/ai-dev
   chmod +x ai/scripts/ai_dev_helper.py
   ```

### Getting Help

1. Check the logs in `.aidev.json`
2. Review AI interaction history
3. Check script output for errors
4. Consult project documentation

## Contributing

1. Add new AI tools to `scripts/`
2. Update templates in `templates/`
3. Document usage in README.md
4. Share successful prompts