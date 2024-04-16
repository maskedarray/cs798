import pandas as pd
import matplotlib.pyplot as plt
import sys

def plot_performance(csv_file_path):
    headers = "DS_TYPENAME TOTAL_THREADS MAXKEY INS DEL prefill_elapsed_ms tree_stats_numNodes total_queries query_throughput total_inserts total_deletes update_throughput total_ops total_throughput".split()
    df = pd.read_csv(csv_file_path, header=None, names=headers, index_col=False)
    df = df[df['MAXKEY'] == 2000000]

    classification_column = 'INS'
    classifications = df[classification_column].unique()

    classified_dfs = {classification: df[df[classification_column] == classification] for classification in classifications}

    x_column = 'TOTAL_THREADS'
    y_column = 'total_throughput'

    fig, axs = plt.subplots(nrows=2, ncols=3, figsize=(18, 16))

    i = 0
    for classification, classified_df in classified_dfs.items():
        ax = axs.flatten()[i]
        i = i + 1
        for type_val, group in classified_df.groupby('DS_TYPENAME'):
            ax.plot(group['TOTAL_THREADS'], group['total_throughput'], label=type_val)
            ax.set_xlabel(x_column)
            ax.set_ylabel(y_column)
            ax.legend()
        ax.set_title(f"INS: {classification} DEL: {classification}")

    fig.suptitle("Performance Comparisons of Data Structures", fontsize=20)
    # plt.tight_layout()
    plt.savefig("output.png")
    

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script_name.py <csv_file_path>")
        sys.exit(1)

    csv_file_path = sys.argv[1]
    plot_performance(csv_file_path)
