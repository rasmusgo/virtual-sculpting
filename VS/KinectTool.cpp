#include "KinectTool.h"
#include "TriangleMesh.h"
#include "KinectReader.h"
#include "GridModel.h"
#include "Shader.h"
#include "GraphicsLib.h"

#include "main.h"

#define PAD_DEPTH 50
#define DIR_Z_STEP 1.0f;

#define FALLBACK_CPU_COUNT 4

static void* run(void* beginning_stop_cpu);


KinectTool::KinectTool()
{
}

KinectTool::KinectTool( float half_x, float half_y, float start_z, float end_z )
{
	_msh = new TriangleMesh(640, 480, -half_x, -half_y, half_x, half_y, start_z);
	_start_z = start_z;
	_end_z = end_z;
	int start_d = 800;

	_reader = new KinectReader(start_d, start_d+int(_start_z - _end_z)*2, _start_z - _end_z );

	_tmp_blured_image = new float[640*480];
	memset( _tmp_blured_image, 0, 640*480*sizeof(float) );

	_tool_shader = new Shader();
	_tool_shader->loadFragmentShader("Shaders/mesh.frag");
	_tool_shader->loadGeometryShader("Shaders/mesh.geom");
	_tool_shader->loadVertexShader("Shaders/mesh.vert");
	_tool_shader->link();
	pvmLocMesh = glGetUniformLocation(_tool_shader->id(), "pvm");

	
	/* Get logical processor count */
	/*cpu_set_t cpu_set;
	int cpu_count = 0;
	CPU_ZERO(&cpu_set);
	sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
	for (cpu_count = 1; cpu_count <= 480; cpu_count++)
	  if (CPU_ISSET(cpu_count, &cpu_set) == 0)
	    break;*/
	cpu_count = 6;
	cpu_count = ((cpu_count <= 0) && (cpu_count <= 480)) ? cpu_count : FALLBACK_CPU_COUNT;
	
	/* Initialise threads  */
	threads = (pthread_t*)malloc(this->cpu_count * sizeof(pthread_t));
	pthread_barrier_init(&(this->barrier), NULL, this->cpu_count + 1);
	
	/* Start threads */
	long beginning = 0, stop, i;
	for (i = 1; i <= cpu_count; i++)
	{
		stop = i * 480 / cpu_count;
		parallellise(i - 1, beginning, stop);
		beginning = stop;
		//sched_setscheduler(0, SCHED_FIFO);
	}
}

Shader* KinectTool::GetToolShader()
{
	return _tool_shader;
}

unsigned int KinectTool::GetPVMLocation()
{
	return pvmLocMesh;
}

inline void Blur( float* in_out, float* tmp )
{
	//static double coeffs[] = {0.0545, 0.224, 0.4026, 0.224, 0.0545};
	static float coeffs[] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
	float summ = 0;
	//X
	int i,j,dx;
	for ( j = 2; j < 478; j++ )	
	{
		for ( i = 0; i < 640; i++ )
		{
			tmp[ j*640 + i ] = 0.0f;
			for ( dx = -2; dx < 3; dx++ )
			{
				tmp[ j*640 + i ] += coeffs[dx+2]*in_out[ (j+dx)*640 + i ];
			}
		}
	}	
	//Y
	for ( j = 2; j < 478; j++ )
	{
		for ( i = 2; i < 638; i++ )
		{
			in_out[ j*640 + i ] = 0.0f;
			for ( dx = -2; dx < 3; dx++ )
			{
				in_out[ j*640 + i ] += (coeffs[dx+2]*tmp[ j*640 + i +dx]);
			}
		}
	}
}

const int eventCount = 1;
HANDLE hEvents[eventCount];

void KinectTool::DoToolUpdate()
{
	_reader->ProcessDepth();//both can be paralelized, but
	//sync point here for process depth.
	//memfence, in a way
	float* depth_ptr = _reader->GetDepth();//Flikers
	//Blur.	
	//Blur( depth_ptr, _tmp_blured_image );//Makes it emen more flikering, but it should be done.
	//memfence again
	_msh->UpdateDepth( depth_ptr );
	//memfence again
	//or you can combine both calls into one ( basically same happens there ), BUT with blur I see no way to do it.
	//So the best way to parralelize it - SIMD-like paralelization inside of each function.

	
}

inline Point Rotate( const Point& v, const glm::quat& q )//looks to be fast.
{
	//quat - point cross
	Point res;
	res.coord[0] = q.y * v.coord[2] - v.coord[1] * q.z;
	res.coord[1] = q.z * v.coord[0] - v.coord[2] * q.x;
	res.coord[2] = q.x * v.coord[1] - v.coord[0] * q.y;
	//again cross
	static Point res1;
	res1.coord[0] = q.y * res.coord[2] - res.coord[1] * q.z;
	res1.coord[1] = q.z * res.coord[0] - res.coord[2] * q.x;
	res1.coord[2] = q.x * res.coord[1] - res.coord[0] * q.y;
	//
	res.coord[0] *= 2.0f*q.w;
	res.coord[1] *= 2.0f*q.w;
	res.coord[2] *= 2.0f*q.w;
	//
	res1.coord[0] *= 2.0f;
	res1.coord[1] *= 2.0f;
	res1.coord[2] *= 2.0f;
	
	res.coord[0] += res1.coord[0] + v.coord[0];
	res.coord[1] += res1.coord[1] + v.coord[1];
	res.coord[2] += res1.coord[2] + v.coord[2];
	return res;
}


static glm::quat inverse;
static Point* points;
static unsigned int grid_dimm;
static Point dir_vector;
static GridModel* grid_model;
static int cpu_output[480];


void KinectTool::StartInteractModel( GridModel* model, glm::quat quat )
{
	//for loop for each point, rotated by inverse of quat
	points = _msh->GetPoints();
	inverse = glm::conjugate(quat);
	

	//Can be easy paralelized. With one but - UpdateCell should be treated properly - it's nor thead safe for moment.

	grid_dimm = model->GetDimm() - 1;

	
	dir_vector.coord[0] = 0.0f;
	dir_vector.coord[1] = 0.0f;
	dir_vector.coord[2] = DIR_Z_STEP;
	dir_vector = Rotate(dir_vector, inverse );
	grid_model = model;
	
	pthread_barrier_wait(&(this->barrier)); /* Start threads */
	

}

int KinectTool::StopInteractModel()
{
	int accum = 0;

	for (long cpu = 0; cpu < this->cpu_count; cpu++)
	  accum += cpu_output[cpu];
	
	pthread_barrier_wait(&(this->barrier)); /* Wait for threads */
	
	return accum;

}

TriangleMesh* KinectTool::GetToolMesh()
{
	return _msh;
}


KinectTool::~KinectTool()
{
	delete _msh;
	delete _reader;
	delete [] _tmp_blured_image;
	delete _tool_shader;
	
	pthread_barrier_destroy(&(this->barrier));
	free(threads);
}



void* SpeechThreed(void* args)
{
	void** args_ = (void**)args;
	KinectTool* self = (KinectTool*)(args_[0]);
	Input* input = (Input*)(args_[1]);
	free(args);

	printf("Thread started.");

	for(;;)
	{
		/* Speech Start */
		hEvents[0] = self->_reader->m_hSpeechEvent;

		// Check to see if we have either a message (by passing in QS_ALLINPUT)
		// Or a speech event (hEvents)
		DWORD dwEvent = MsgWaitForMultipleObjectsEx(eventCount, hEvents, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

		// Check if this is an event we're waiting on and not a timeout or message
		if (WAIT_OBJECT_0 == dwEvent)
		{
			self->_reader->ProcessSpeech(input);
		}
	/*
	if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if ((hWndApp != NULL) && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
		*/
	/* Speech end */
	}

	return NULL;
}

void KinectTool::parallellise(long cpu, long beginning, long stop)
{
	long args_0 = (beginning | (stop << 9) | (cpu << 18));
	long args_1 = (long)(this);
	long* args = new long[2];
	args[0] = args_0;
	args[1] = args_1;
	if ((errno = pthread_create(this->threads + cpu, NULL, run, args)))
	{
		perror("pthread_create");
		abort();
	}
	else
	{
	}
}


static void* run(void* args)
{
	KinectTool* tool = (KinectTool*)(((long*)args)[1]);

	long beginning = long(((long*)args)[0]) & 511;
	long stop = (long(((long*)args)[0]) >> 9) & 511;
	long cpu = (long(((long*)args)[0]) >> 18) & 511;
	delete args;

	long count, x, y;
	int accum = 0;
  
	Point action_point;
	unsigned int tmp1, tmp2, tmp3;
	Point tmp;
	UINT8 val = 128;
  
	for (;;)
	{
		pthread_barrier_wait(&(tool->barrier)); /* Wait for start cue */
		accum = 0;
		Point local_dir_vector = dir_vector;
		count = 0;
		for (y = beginning; y < stop; y++)
		for (x = 0; x < 640; x++)
		{
			//tmp = points[ x*480 + y ];
			//tmp.coord[2] -= DIR_Z_STEP * PAD_DEPTH;
			action_point = Rotate( points[ x*480 + y ], inverse);
			for ( int delta = 0; delta < PAD_DEPTH; delta++ )
			{
				tmp.coord[0] = action_point.coord[0] + local_dir_vector.coord[0]*delta;
				tmp.coord[1] = action_point.coord[1] + local_dir_vector.coord[1]*delta;
				tmp.coord[2] = action_point.coord[2] + local_dir_vector.coord[2]*delta;
				/*index =*/ grid_model->GetCellIndex(tmp, tmp1, tmp2, tmp3);
		
				if (!( ( tmp1 > grid_dimm ) || ( tmp2 > grid_dimm ) || ( tmp3 > grid_dimm )))//if we are in model bounds
				{
					accum += grid_model->UpdateCellMelt(tmp1, tmp2, tmp3, val);
				}
				else
					break;
			}
		}
      
		  cpu_output[cpu] = accum;
      
		  pthread_barrier_wait(&(tool->barrier)); /* Signal that thread complete */
    }
  return NULL;
}

