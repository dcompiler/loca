# Using thin webserver
# gem install thin --user-install
# to run: thin start -R server.ru --threaded

$Debug = true
$DryRun = false

LocaServerDir = File.dirname(File.expand_path(__FILE__))
$: << LocaServerDir

## Look for the data directory:
#  (1) choose the current directory if it is named 'data'.
#  (2) choose ~/.loca/data if env.rb is available (i.e.
#  client/server on the same machine).
#  (3) if the first two fail, use the current directory, whatever it is.
if File.expand_path(".").split("/")[-1] == 'data'
  HomeDir = File.expand_path(".")
else
  begin
    require '../gem/lib/src/env'
    data_dir = Labenv.paths[Labenv::DataDir]
  rescue Exception
  end
  if data_dir != nil
    HomeDir = data_dir
  else
    HomeDir = File.expand_path(".")
  end
end
### $currentDir = "./"
puts "LocaServer: serving from #{HomeDir}"

FileUtils.cp_r("#{LocaServerDir}/static/.", \
								 "#{HomeDir}/.static/")

require 'server'
require 'plot'
require 'fileutils'

module Util
  def self.run_cmd cmd
    puts cmd
    system cmd unless $DryRun
  end
end

class Rack::Request
  include Plot
end

class CServer

  def call(env)
    request = Rack::Request.new(env) # request ex. #<Rack::Request:0x101c62da0>
    req = request.path # req looks like /program.dat or foldername
    fullreq = File.join(HomeDir, req) # fullreq is path/req
    ext = File.extname(req)
    currentDir = File.join("./", File.dirname(req)) #{}"./"

    ## Create list of items in directory.
    if File.directory?( fullreq )
      if req == "/"
        currentDir = "./"
      else
        currentDir = File.join("./", req)
      end

      files, dirs = Server.browse(currentDir)
      htmlString  = Server.headMatter(currentDir)
      htmlString += Server.tableSection(currentDir, files, dirs)
      htmlString += Server.footMatter()

      htmlFile = File.new(".current.html", "w")
      htmlFile.write(htmlString)
      htmlFile.close

      contents = File.open(".current.html",'r') {|f| f.read}
      headers = {
          'Content-Type' => 'text/html',
          'Content-Length' => contents.bytesize.to_s,
      }
      ## Create contents of the plotting page.
    elsif ext == ".dat" or ext == ".fp"
      program = File.basename(req, ext) # strips .dat or .fp from req.
      ## Create HTML files to display dynamic picture.
      ["fp", "lf", "mr", "rd"].each do |metric|
        File.open( "#{program}.#{metric}.html", "w" ) do |imageHTML|
          imageHTML.puts Server.imagePage(currentDir, "#{program}", "#{metric}")
        end
      end

      ## Plot the graphs.
      # If .png file doesn't exist, make it.
      if ! File.file?( File.join(currentDir, "#{program}.fp.png") )
        Plot.default(currentDir, program, ext)
      end

      ## If the request is a post, read form data and plot
      #  the zoomed images.
      if (request.post?)
        ## If the range form has been submitted,
        #  plot that range and go to the same plot.
        #  Else If the plot type form has been submitted, go to that plot.
        if (request.params["whichForm"] == "rangeForm")
          program = request.params["program"]
          metric = request.params["metric"]
          xmin = request.params["xmin"].to_f
          xmax = request.params["xmax"].to_f
          Plot.zoom(currentDir, program, ext, xmin, xmax, metric)
          plothtml = "#{program}.#{metric}.html"
          contents = File.open( plothtml, 'r') {|f| f.read}
        elsif (request.params["whichForm"] == "typeForm")
          goToMetric = request.params["goToMetric"]
          contents = File.open("#{program}.#{goToMetric}.html", 'r') {|f| f.read}
        end
      else
        contents = File.open("#{program}.fp.html", 'r') {|f| f.read}
      end

      ## Create headers.
      headers = {
          'Content-Type'   => 'text/html',
          'Content-Length' => contents.bytesize.to_s,
      }

    else
      req = (req == '/') ? '.current.html' : ".#{req}"
      type = case req
               when /\.html$/ then 'text/html'
               when /\.gif$/ then 'image/gif'
               when /\.ico$/ then 'image/x-icon'
               when /\.pdf$/ then 'image/png'
               else 'text/plain'
             end

      return unless File.file?(req)
      contents = File.open(req,'r') {|file| file.read}
      headers = {
          'Content-Type' => type,
          'Content-Length' => contents.bytesize.to_s,
      }
    end
    response = [200, headers, [contents]]

  end
end
class UpServer
  def call(env)
    request = Rack::Request.new(env)
    name = ''
    if(request.path == '/upload')
      name = request.params["upload"][:filename]
      puts name
      if (name =~ /.dat$/ or name =~ /.info$/) #check file type
        res = File.new("#{HomeDir}/data/#{name}","wb")
        res.puts request.params["upload"][:tempfile].readlines
        res.close
      end
    end
    contents = ''
    if name =~ /.dat$/
      contents = "http://localhost:3000/data/#{name}"
    end
    headers = {
        'Content-Length' => contents.bytesize.to_s,
    }
    response = [200, headers, [contents]]
  end
end

s = CServer.new
up = UpServer.new

app = Rack::Builder.new do
  map('/') {run s}
  map('/upload') {run up}
end

run app
