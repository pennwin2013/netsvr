#ifndef _SUBPROCESS_HPP_

#define _SUBPROCESS_HPP_

#include <string>

#include <vector>



#ifdef WIN32

#include <Windows.h>

#else

#include <unistd.h>

#endif



namespace subprocess

{



class status;



class child

{

 public:

#ifdef WIN32

  typedef DWORD pid_type;

#else

  typedef pid_t pid_type;

#endif



#ifdef WIN32

  explicit child(HANDLE h = INVALID_HANDLE_VALUE);

#else

  explicit child(pid_type pid = get_invalidate_pid());

#endif



  ~child();

  child(const child& rhs);

  child& operator=(const child& rhs);



  inline pid_type get_pid() const

  {

    return pid_;

  }



  status wait();

  

  bool terminate();



  bool alive();



  static pid_type get_invalidate_pid()

  {

#ifdef WIN32

    return -1;

#else

    return (pid_t)-1;

#endif

  }

 private:

  pid_type pid_;

#ifdef WIN32

  HANDLE handle_;

#endif

};



class status

{

 public:

#ifdef WIN32

  typedef DWORD exit_code_type;

#else

  typedef int exit_code_type;

#endif

  explicit status(const child& c,exit_code_type code);

  ~status();

  exit_code_type exit_code() const

  {

    return exit_code_;

  }

 private:

  exit_code_type exit_code_;

  child child_;

};





class context

{

 public:

  context();

  ~context();

  std::string working_dir();

  void set_working_dir(const std::string &dir);

 private:

  std::string working_dir_;

};



class command_line

{

 public:

  static const std::size_t kMaxCommandArgs=256;

  typedef std::vector<std::string> ArgumentVector;

  typedef ArgumentVector::const_iterator ArgumentItertor;



  command_line(const std::string& cmd);

  ~command_line();

  command_line& argument(const std::string& arg);

  ArgumentVector::const_iterator begin() const;

  ArgumentVector::const_iterator end() const;

  std::string command() const

  {

    return command_;

  }

 private:

  

  ArgumentVector args_;

  std::string command_;



};



class launcher

{

 public:

  explicit launcher();

  ~launcher();

  child start(const command_line &cmd_line,context &ctx);

  child start(const command_line &cmd_line);

};



}



#endif // _SUBPROCESS_HPP_

