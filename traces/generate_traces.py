import json
import random

def generate_workload(num=500, filename="workload.json"):
    trace = []
    operators = ["vector_add", "matmul", "conv2d"]
    
    for i in range(num):
        input_size = random.choice([512, 1024, 2048, 4096, 8192, 16384, 32768])
        priority = random.randint(0, 3)
        op = random.choice(operators)
        
        trace.append({
            "id": i + 1,
            "input_size": input_size,
            "priority": priority,
            "operator": op
        })
    
    with open(filename, "w") as f:
        json.dump(trace, f, indent=2)
    
    print(f"生成完成！共 {num} 条请求，已保存到 {filename}")

if __name__ == "__main__":
    generate_workload(1000)