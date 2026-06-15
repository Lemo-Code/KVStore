import re
# config.cc - fix broken fprintf replacement
with open("config/config.cc", "r") as f:
    c = f.read()
c = c.replace('ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "Config::LoadFromYaml error: " << e.what();', 'fprintf(stderr, "Config::LoadFromYaml error: %s\\n", e.what());')
with open("config/config.cc", "w") as f:
    f.write(c)
# zero.cc - restore
with open("zero.cc", "r") as f:
    c = f.read()
c = c.replace('ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "hook_init: failed to find " << #name;', 'fprintf(stderr, "hook_init: failed to find %s\\n", #name);')
with open("zero.cc", "w") as f:
    f.write(c)
print("fixed")
