#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// Struct com as informações solicitadas de uma tarefa
typedef struct {
    const char* Nome;
    const char* Estado;
    uint32_t Prioridade;
    uint16_t Tamanho;
    int Nucleo;
}parametrosTarefa;

//Funcao que permite converter os estados da tarefas em const char*
const char* estadoTarefa(eTaskState estado) {
    switch (estado) {
        case eRunning:   return "Executando";
        case eReady:     return "Pronta";
        case eBlocked:   return "Bloqueada";
        case eSuspended: return "Suspensa";
        case eDeleted:   return "Deletada";
        default:         return "Desconhecido";
    }
}

// Task to be created.
void vTaskGetDescription( void * pvParameters )
{
for( ;; )
  {
    volatile UBaseType_t x;
    unsigned long ulTotalRunTime, ulStatsAsPercentage;

    // Salva o numero de tarefas
    UBaseType_t uxArraySize  = uxTaskGetNumberOfTasks();

    //Aloca a memorir para cada tarefa
    TaskStatus_t *pxTaskStatusArray = pvPortMalloc(uxArraySize  * sizeof(TaskStatus_t));

    if(pxTaskStatusArray != NULL){

        uint32_t ulTotalRunTime;

       // Gera as informações do estatus de cada task
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime);    

            printf("\n--- Monitoramento (Total de Tarefas: %d) ---\n", uxArraySize);

            // Loop para imprimir os dados de CADA tarefa
            for( UBaseType_t x = 0; x < uxArraySize; x++ )
            {
                parametrosTarefa Tarefa;

                Tarefa.Nome = pxTaskStatusArray[x].pcTaskName;
                Tarefa.Estado = estadoTarefa(pxTaskStatusArray[x].eCurrentState);
                Tarefa.Prioridade = pxTaskStatusArray[x].uxCurrentPriority;
                Tarefa.Tamanho = pxTaskStatusArray[x].usStackHighWaterMark;
                Tarefa.Nucleo = xTaskGetCoreID(pxTaskStatusArray[x].xHandle);

                // Verificando a afinidade do núcleo para não imprimir números estranhos
                if (Tarefa.Nucleo == tskNO_AFFINITY) {
                    printf("Nome: %-16s | Prio: %lu | Tam: %-5u | Núcleo: Qualquer | Estado: %s\n", 
                           Tarefa.Nome, Tarefa.Prioridade, Tarefa.Tamanho, Tarefa.Estado);
                } else {
                    printf("Nome: %-16s | Prio: %lu | Tam: %-5u | Núcleo: Core %d   | Estado: %s\n", 
                           Tarefa.Nome, Tarefa.Prioridade, Tarefa.Tamanho, Tarefa.Nucleo, Tarefa.Estado);
                }
            }

        // The array is no longer needed, free the memory it consumes.
        vPortFree( pxTaskStatusArray );
    }
    // PAUSA DE 5 SEGUNDOS (Requisito da atividade e salva o ESP32 de travar)
        vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void app_main(void)
{
    //Cria a tarefa de monitoreamento.
    xTaskCreate(vTaskGetDescription, "MonitorTarefas", 4096, NULL, 5, NULL);
}