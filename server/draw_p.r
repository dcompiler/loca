args <- commandArgs(TRUE)

program <- args[1]
ext <- args[2]

rawdata <- read.table(paste(program, ext, sep=""))

#draw footprint curve
saveto <- sprintf("%s.fp.png", program)
x <- rawdata[,1]
y <- rawdata[,2]*64 # "* 64" converts to bytes
xlab <- "window size (accesses)"
ylab <- "footprint (bytes)"
legend <- c("average footprint")
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab,
	frame.plot=TRUE, col="red", main="Average Footprint")
dev.off()

#draw lifetime curve
saveto <- sprintf("%s.lf.png", program)
x <- rawdata[,2]*64
y <- rawdata[,1]
xlab <- "cache size (bytes)"
ylab <- "lifetime (accesses)"
legend <- c("lifetime")
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab,
	frame.plot=TRUE, col="red", main="Data Lifetime in Cache")
dev.off()